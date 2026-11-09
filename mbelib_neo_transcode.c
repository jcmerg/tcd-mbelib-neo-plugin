// SPDX-License-Identifier: GPL-3.0-or-later
//
// transcode for the mbelib-neo plugin: the frame a decoder last decoded,
// coded again by an encoder of another MBE codec from the model, without
// synthesis and analysis, on mbelib-neo MBE_TRANSCODE_MBELIB
// (mbelib_neo_transcode.h).
//
// The three vocoders share the MBE model: a fundamental, a voicing decision
// per harmonic and a log2 amplitude per harmonic. The decoder's model is
// mapped onto the target's pitch grid (each target harmonic takes the source
// harmonic nearest in frequency), the target's voicing bands or columns are
// decided by an energy-weighted vote of the harmonics they cover, and the
// amplitudes are quantized with the target's frame coder against the
// encoder's own prediction history, which follows the decoder's model of the
// frames sent, as mbelib-neo's PCM encoders keep it.
//
// Levels: AMBE and AMBE+2 keep log2Ml before the unvoiced factor
// 0.2046 / sqrt(w0) their synthesis applies, IMBE after it, so the model is
// carried at the synthesized level and converted on each side. The gain is
// added there, 6.02 dB per unit of log2.
//
// Tones: a D-Star or AMBE+2 tone frame becomes the target's tone frame when
// the target has one for its frequencies, at the same level; otherwise (IMBE,
// KNOX tones into D-Star) it is played as voiced harmonics of a fundamental
// chosen so that the tone frequencies fall on harmonics.
//
// mbelib-neo's public API encodes from PCM only. The quantizers are in its
// static archive as ordinary symbols (its private API, src/internal), and what
// is static there is here: D-Star's pitch code, quantization and packing, and
// the tone frames. Both are of that one version; the Makefile and
// mbelib_neo_plugin.cpp refuse another.
//
// Adapted from mbelib-neo (https://github.com/arancormonk/mbelib-neo,
// GPL-2.0-or-later, used here under GPL-3.0-or-later):
//   ambe2400_pitch_code, ambe2400_pack, ambe2400_quantize, ambe2400_pack_tone
//   from src/ambe/ambe3600x2400_enc.c,
//     Copyright (C) 2026 Rhizomatica
//     Copyright (C) 2010 mbelib Author (portions originally under the ISC license)
//   ambe2450_pack_tone, ambe2450_unpack_tone from src/ambe/ambe3600x2450_enc.c
//   and imbe_pitch_code and code_imbe's pitch and gain floor from
//   src/imbe/imbe7200x4400_enc.c,
//     Copyright (C) 2026 by arancormonk <180709949+arancormonk@users.noreply.github.com>

#include <math.h>
#include <string.h>

#include "ambe3600x2400_internal.h"
#include "ambe3600x2450_internal.h"
#include "ambe_common.h"
#include "ambe_encoder.h"
#include "imbe4400_internal.h"
#include "imbe7200x4400_const.h"
#include "mbe_encoder.h"
#include "mbe_speech_analysis.h"
#include "mbe_tone.h"

#include "mbelib_neo_transcode.h"

#define MBE_TRANSCODE_MODEL_MUTE  0
#define MBE_TRANSCODE_MODEL_VOICE 1
#define MBE_TRANSCODE_MODEL_TONE  2

#define LOG2_PER_DB (1.0f / 6.0206f)
// the gain range: beyond it every frame is silence or clips
#define GAIN_DB_MAX 60.0f
// AMBE's unvoiced factor, 0.2046 / sqrt(w0) (mbe_ambe2400_update_spectral_amplitudes)
#define UNVOICED_COEFF 0.2046f
// What a tone played as a voiced harmonic loses to each decoder's
// quantization and enhancement, in log2, measured with a 1 kHz tone at
// -20 dBFS: AMBE, AMBE+2, IMBE.
static const float tone_log2_offset[3] = { -1.09f, -0.51f, -1.08f };
// Harmonics of a tone frame played as voice, other than the tone, lie this
// far below it: 30 dB, which the amplitude quantizers still shape around a
// lone harmonic.
#define TONE_FLOOR_LOG2 5.0f
// log2 amplitude of every harmonic of the quiet model: below the lowest
// level of every gain table (AMBE, AMBE+2 and IMBE), so each coder fits it
// to its own floor (mbe_encoder_fit_floor()), the quietest frame it has.
#define QUIET_LOG2 (-8.0f)

// the model at the synthesized level: f0 in cycles per sample, a[l] the log2
// amplitude each harmonic plays at
struct model
{
	float f0;
	int L;
	int Vl[57];
	float a[57];
};

static float unvoiced_log2(float f0)
{
	return log2f(UNVOICED_COEFF / sqrtf(2.0f * (float)M_PI * f0));
}

static int clamp_l(int L)
{
	return (L < 9) ? 9 : ((L > 56) ? 56 : L);
}

static int clamp_int(long v, int lo, int hi)
{
	return (int)((v < lo) ? lo : ((v > hi) ? hi : v));
}

// --- the decoder's side ----------------------------------------------------

void mbe_transcodeForget(struct mbe_transcode_model *m)
{
	memset(m, 0, sizeof(*m));
}

// The tone id and AD of an AMBE+2 tone frame (TIA-102.BABA-1 7.2 Table 10).
static void ambe2450_unpack_tone(const char ambe_d[49], int *tone_id, int *ad)
{
	int id = 0, a = 0;
	for (int i = 12; i < 20; i++)
		id = (id << 1) | (ambe_d[i] & 1);
	for (int i = 6; i < 12; i++)
		a = (a << 1) | (ambe_d[i] & 1);
	*tone_id = id;
	*ad = (a << 1) | (ambe_d[44] & 1);
}

// What the frame just processed leaves: the model of a voice, silence or
// repeated frame; a tone's frequencies and level; or the mute.
void mbe_transcodeRecord(struct mbe_transcode_model *m, int vocoder, const mbe_process_result *result, const char *bits,
	const mbe_parms *cur, const mbe_parms *prev)
{
	const unsigned flags = result->flags;
	if (flags & MBE_PROCESS_FLAG_MUTE)
	{
		m->kind = MBE_TRANSCODE_MODEL_MUTE;
		return;
	}
	// A repeat (a lost frame, an erasure, an uncorrectable C0 word) plays the
	// last frame again: the model stays the one recorded for it. The
	// decoder's prev can hold another frame by then (the last voice frame
	// after a silence or tone frame, or the corrupt frame's amplitudes D-Star
	// keeps for its prediction).
	if (flags & (MBE_PROCESS_FLAG_REPEAT | MBE_PROCESS_FLAG_ERASURE))
	{
		// after a tone the decoder plays silence, except D-Star's two-tone
		// signals, which it holds over a few lost frames
		if (m->kind == MBE_TRANSCODE_MODEL_TONE && !m->tone_repeats)
			m->kind = MBE_TRANSCODE_MODEL_MUTE;
		return;
	}
	if (flags & MBE_PROCESS_FLAG_TONE)
	{
		float f1 = 0.0f, f2 = 0.0f;
		int ok = 0;
		m->tone_repeats = 0;
		if (vocoder == MBE_TRANSCODE_AMBE2400)
		{
			const int index = mbe_ambe2400_tone_index(bits);
			ok = mbe_tone_lookup_dstar_freqs(index, &f1, &f2);
			m->tone_db = (float)mbe_tone_dstar_level_db(mbe_tone_dstar_volume(bits));
			m->tone_repeats = index >= 128;
		}
		else if (vocoder == MBE_TRANSCODE_AMBE2450)
		{
			int id = 0, ad = 0;
			ambe2450_unpack_tone(bits, &id, &ad);
			if (id == 255)
			{
				m->kind = MBE_TRANSCODE_MODEL_MUTE;	// the zero-amplitude tone: silence
				return;
			}
			ok = mbe_tone_lookup_freqs(id, &f1, &f2);
			m->tone_db = (float)mbe_tone_ambe2450_level_db(ad);
		}
		if (ok)
		{
			m->kind = MBE_TRANSCODE_MODEL_TONE;
			m->tone_f1 = f1;
			m->tone_f2 = f2;
			return;
		}
	}
	// a silence frame's model is decoded but kept out of the prediction
	const mbe_parms *src = (flags & MBE_PROCESS_FLAG_SILENCE) ? cur : prev;
	m->kind = MBE_TRANSCODE_MODEL_VOICE;
	m->w0 = src->w0;
	m->L = (src->L < 1) ? 1 : ((src->L > 56) ? 56 : src->L);
	for (int l = 1; l <= m->L; l++)
	{
		m->Vl[l] = src->Vl[l] != 0;
		m->log2Ml[l] = src->log2Ml[l];
	}
}

// --- models ----------------------------------------------------------------

// the quietest model: AMBE's silence frame, every harmonic at the floor
static void quiet_model(struct model *m)
{
	m->f0 = MBE_AMBE_SILENCE_W0 / (2.0f * (float)M_PI);
	m->L = MBE_AMBE_SILENCE_L;
	for (int l = 1; l <= m->L; l++)
	{
		m->Vl[l] = 0;
		m->a[l] = QUIET_LOG2;
	}
}

static void voice_model(const struct mbe_transcode_model *src, int vocoder, float gain_log2, struct model *m)
{
	m->f0 = src->w0 / (2.0f * (float)M_PI);
	m->L = src->L;
	const float unvc = (vocoder == MBE_TRANSCODE_IMBE) ? 0.0f : unvoiced_log2(m->f0);
	for (int l = 1; l <= m->L; l++)
	{
		m->Vl[l] = src->Vl[l];
		m->a[l] = src->log2Ml[l] + gain_log2 + (m->Vl[l] ? 0.0f : unvc);
	}
}

// D-Star's voice pitch code nearest f0 (as mbelib-neo's ambe2400_enc_quantize_pitch)
static int ambe2400_pitch_code(float f0)
{
	int b0 = (int)lroundf(((MBE_AMBE2400_LOG2_F0_OFFSET - log2f(f0)) / MBE_AMBE2400_LOG2_F0_SLOPE) - 0.5f);
	if (b0 < 0)
		b0 = 0;
	// DVSI's encoder sends no voice frame above b0 123, and its decoder plays 125 as a lost frame
	if (b0 > MBE_AMBE2400_MAX_VOICE_B0)
		b0 = MBE_AMBE2400_MAX_VOICE_B0;
	return b0;
}

// TIA-102.BABA 6.1 eq 45: IMBE's b0 from the pitch period 2 / f0 in half samples
static int imbe_pitch_code(float f0)
{
	const long b0 = lround(floor((2.0 / (double)f0) - 39.0));
	return (int)((b0 < 0) ? 0 : ((b0 > 207) ? 207 : b0));
}

// the fundamental (cycles per sample) the target codes nearest to f0
static float quantized_f0(float f0, int vocoder)
{
	if (vocoder == MBE_TRANSCODE_AMBE2400)
		return mbe_ambe2400_f0(ambe2400_pitch_code(f0));
	if (vocoder == MBE_TRANSCODE_AMBE2450)
		return mbe_ambe2450_f0(mbe_ambe2450_pitch_code(f0));
	return 2.0f / ((float)imbe_pitch_code(f0) + 39.5f);
}

// the harmonics the target codes at the fundamental f0 (cycles per sample) of its pitch grid
static int harmonic_count(float f0, int vocoder)
{
	if (vocoder == MBE_TRANSCODE_AMBE2400)
		return mbe_ambe2400_harmonic_count(f0);
	if (vocoder == MBE_TRANSCODE_AMBE2450)
		return mbe_ambe2450_harmonic_count(mbe_ambe2450_pitch_code(f0));
	const float w0 = 2.0f * (float)M_PI * f0;
	return clamp_l((int)(0.9254 * (int)((M_PI / w0) + 0.25)));
}

// Tone frequencies as voiced harmonics: one tone at the harmonic k, two at
// the harmonics k1 and k2, of the fundamental on the target's pitch grid
// (65..400 Hz) that misses the tones least and has them within its harmonic
// count. 0 when there is none (a tone above about 3.7 kHz).
static int tone_model(float f1, float f2, float db, int vocoder, struct model *m)
{
	const float level = log2f(32768.0f * (float)M_SQRT2 * powf(10.0f, db / 20.0f)) + tone_log2_offset[vocoder];
	const int dual = f2 > 0.0f && fabsf(f2 - f1) > 1.0f;
	int k1 = 1, k2 = 0;
	float f0 = f1 / 8000.0f;
	float best = 1.0e9f;
	for (int i = 1; i <= 56; i++)
	{
		for (int j = dual ? 1 : 0; j <= (dual ? 56 : 0); j++)
		{
			const float c = dual ? 0.5f * ((f1 / (float)i) + (f2 / (float)j)) : f1 / (float)i;
			if (c < 65.0f || c > 400.0f)
				continue;
			const float q = quantized_f0(c / 8000.0f, vocoder) * 8000.0f;
			const int L = harmonic_count(q / 8000.0f, vocoder);
			if (i > L || j > L)
				continue;
			// the miss, plus a preference for few harmonics: a lone high
			// harmonic among many loses level to the amplitude quantizer
			float err = (fabsf(f1 - ((float)i * q)) / f1) + (0.002f * (float)(i + j));
			if (dual)
				err += fabsf(f2 - ((float)j * q)) / f2;
			if (err < best)
			{
				best = err;
				k1 = i;
				k2 = j;
				f0 = q / 8000.0f;
			}
		}
	}
	if (best >= 1.0e9f)
		return 0;
	m->f0 = f0;
	m->L = harmonic_count(f0, vocoder);
	for (int l = 1; l <= m->L; l++)
	{
		m->Vl[l] = 0;
		m->a[l] = level - TONE_FLOOR_LOG2;
	}
	m->Vl[k1] = 1;
	m->a[k1] = level;
	if (dual)
	{
		m->Vl[k2] = 1;
		m->a[k2] = level;
	}
	return 1;
}

// target harmonic l at fundamental f0t takes the source harmonic nearest in frequency
static int source_harmonic(const struct model *s, float f0t, int l)
{
	const int ls = (int)lroundf((float)l * f0t / s->f0);
	return (ls < 1) ? 1 : ((ls > s->L) ? s->L : ls);
}

// the decision of a voicing group by the energy of its harmonics
struct vote
{
	float voiced[12];
	float unvoiced[12];
};

static void vote_add(struct vote *v, int group, float a, int voiced)
{
	const float e = exp2f(2.0f * a);
	if (voiced)
		v->voiced[group] += e;
	else
		v->unvoiced[group] += e;
}

// --- AMBE+2 ------------------------------------------------------------------

static int code_ambe2450(const struct model *s, mbe_parms *cur, const mbe_parms *prev, char bits[49])
{
	const int b0 = mbe_ambe2450_pitch_code(s->f0);
	const float f0q = mbe_ambe2450_f0(b0);
	const int L = mbe_ambe2450_harmonic_count(b0);
	float a[57] = { 0 };
	float magnitude[57] = { 0 };
	unsigned char bands[12];
	struct vote v = { { 0 }, { 0 } };
	for (int l = 1; l <= L; l++)
	{
		const int ls = source_harmonic(s, f0q, l);
		a[l] = s->a[ls];
		magnitude[l] = exp2f(a[l]);
		vote_add(&v, mbe_analysis_band_of(l) - 1, a[l], s->Vl[ls]);
	}
	for (int b = 0; b < 12; b++)
		bands[b] = (unsigned char)(v.voiced[b] > v.unvoiced[b]);
	// The unvoiced factor is taken out by the band decisions, not by the
	// quantized V/UV vector the decoder applies it by: a harmonic the vector
	// unvoices against its band plays a little quieter as noise than it was
	// as a sinusoid; taken out by the vector instead it measures lower.
	const float unvc = unvoiced_log2(f0q);
	for (int l = 1; l <= L; l++)
		if (!bands[mbe_analysis_band_of(l) - 1])
			a[l] -= unvc;
	const int b1 = mbe_ambe2450_quantize_vuv(b0, magnitude, bands);
	mbe_ambe2450_quantize_amplitudes(b0, b1, a, prev, bits);
	// the decoder's model of the frame, for the prediction of the next
	mbe_parms history = *prev;
	return mbe_decodeAmbe2450Parms(bits, cur, &history) == MBE_AMBE2450_FRAME_VOICE ? 0 : -1;
}

// --- AMBE (D-Star) -------------------------------------------------------------

// the 49 bits as mbelib-neo's ambe2400_enc_pack
static void ambe2400_pack(const int b[9], char ambe_d[49])
{
	static const signed char b2pos[6] = { 6, 7, 8, 9, 42, 43 };
	static const signed char b3pos[9] = { 10, 11, 12, 13, 14, 15, 16, 44, 45 };
	static const signed char b4pos[7] = { 17, 18, 19, 20, 21, 46, 47 };
	static const signed char b5pos[4] = { 22, 23, 25, 26 };	// 24 is the spare bit
	memset(ambe_d, 0, 49);
	for (int i = 0; i < 6; i++)
		ambe_d[i] = (char)((b[0] >> (6 - i)) & 1);
	ambe_d[48] = (char)(b[0] & 1);
	for (int i = 0; i < 4; i++)
		ambe_d[38 + i] = (char)((b[1] >> (3 - i)) & 1);
	for (int i = 0; i < 6; i++)
		ambe_d[b2pos[i]] = (char)((b[2] >> (5 - i)) & 1);
	for (int i = 0; i < 9; i++)
		ambe_d[b3pos[i]] = (char)((b[3] >> (8 - i)) & 1);
	for (int i = 0; i < 7; i++)
		ambe_d[b4pos[i]] = (char)((b[4] >> (6 - i)) & 1);
	for (int i = 0; i < 4; i++)
	{
		ambe_d[b5pos[i]] = (char)((b[5] >> (3 - i)) & 1);
		ambe_d[27 + i] = (char)((b[6] >> (3 - i)) & 1);
		ambe_d[31 + i] = (char)((b[7] >> (3 - i)) & 1);
	}
	for (int i = 0; i < 3; i++)	// block 4 carries only bits 3..1
		ambe_d[35 + i] = (char)((b[8] >> (3 - i)) & 1);
}

// b2..b8 for log2 amplitudes a (the decoder's domain, less 0.5 log2 L) of
// pitch b0 and voicing b1 against prev, packed, and the decoder's model of
// them in cur: mbelib-neo's ambe2400_encode_voice after its analysis
static void ambe2400_quantize(int b0, int b1, const float a[57], const mbe_parms *prev, mbe_parms *cur, char ambe_d[49])
{
	struct ambe_enc_frame q;
	memset(&q, 0, sizeof(q));
	const struct ambe_enc_tables *t = mbe_ambe2400_enc_tables();
	mbe_parms prediction_prev = *prev;
	mbe_ambe2400_resolve_history(&prediction_prev);
	q.cache = mbe_ambe2400_get_dct_cache();
	q.b[0] = b0;
	q.f0q = mbe_ambe2400_f0(b0);
	q.L = mbe_ambe2400_harmonic_count(q.f0q);
	q.b[1] = b1 & 0xF;
	memcpy(q.a, a, sizeof(q.a));
	ambe_enc_mean_amplitude(&q);
	ambe_enc_quantize_gain(&q, t, &prediction_prev);
	ambe_enc_prediction(&q, t, &prediction_prev);
	ambe_enc_quantize_residual(&q, t);
	float Ri_q[9];
	mbe_ambe2400_reconstruct_prba(q.b[3], q.b[4], Ri_q);
	mbe_ambe2400_reconstruct_cik(Ri_q, q.b + 5, q.Ji, q.Cik_q);
	mbe_ambe2400_inverse_dct_tl(q.Cik_q, q.Ji, q.Tl_q);
	ambe2400_pack(q.b, ambe_d);

	cur->w0 = q.f0q * 2.0f * (float)M_PI;
	cur->L = q.L;
	cur->K = 0;
	cur->mutingThreshold = MBE_MUTING_THRESHOLD_AMBE;
	for (int l = 1; l <= q.L; l++)
	{
		cur->Vl[l] = mbe_ambe2400_harmonic_voiced(q.b[1], l, q.f0q);
		if (cur->Vl[l] == 1)
			cur->K++;
	}
	cur->gamma = q.gamma_q;
	mbe_ambe2400_update_spectral_amplitudes(cur, &prediction_prev, q.Tl_q, UNVOICED_COEFF / sqrtf(cur->w0));
	cur->Vl[0] &= ~MBE_AMBE2400_HISTORY_BITS;
}

static int code_ambe2400(const struct model *s, mbe_parms *cur, const mbe_parms *prev, char bits[49])
{
	const int b0 = ambe2400_pitch_code(s->f0);
	const float f0q = mbe_ambe2400_f0(b0);
	const int L = mbe_ambe2400_harmonic_count(f0q);
	float a[57] = { 0 };
	struct vote v = { { 0 }, { 0 } };
	for (int l = 1; l <= L; l++)
	{
		const int ls = source_harmonic(s, f0q, l);
		a[l] = s->a[ls];
		const int column = (int)((float)l * 16.0f * f0q);
		vote_add(&v, (column > 7) ? 7 : column, a[l], s->Vl[ls]);
	}
	// b1: one bit per 1 kHz, two columns each
	int b1 = 0;
	for (int bit = 0; bit < 4; bit++)
	{
		const int c = 2 * bit;
		const int voiced = v.voiced[c] + v.voiced[c + 1] > v.unvoiced[c] + v.unvoiced[c + 1];
		b1 |= voiced << (3 - bit);
	}
	const float unvc = unvoiced_log2(f0q);
	for (int l = 1; l <= L; l++)
		if (!mbe_ambe2400_harmonic_voiced(b1, l, f0q))
			a[l] -= unvc;
	ambe2400_quantize(b0, b1, a, prev, cur, bits);
	return 0;
}

// --- IMBE --------------------------------------------------------------------

static int code_imbe(const struct model *s, mbe_parms *cur, const mbe_parms *prev, int sync, char bits[88])
{
	const int b0 = imbe_pitch_code(s->f0);
	const float w0 = (float)(4 * M_PI) / ((float)b0 + 39.5f);
	const float f0q = w0 / (2.0f * (float)M_PI);
	const int L = clamp_l((int)(0.9254 * (int)((M_PI / w0) + 0.25)));
	float logm[57] = { 0 };
	unsigned char bands[12] = { 0 };
	struct vote v = { { 0 }, { 0 } };
	float mean = 0.0f;
	for (int l = 1; l <= L; l++)
	{
		const int ls = source_harmonic(s, f0q, l);
		logm[l] = s->a[ls];
		mean += logm[l];
		vote_add(&v, mbe_analysis_band_of(l) - 1, logm[l], s->Vl[ls]);
	}
	for (int b = 0; b < 12; b++)
		bands[b] = (unsigned char)(v.voiced[b] > v.unvoiced[b]);
	// G1 carries the mean log2 amplitude, and the gain table's lowest level
	// is the floor under it (as mbelib-neo's imbe_encode)
	mean /= (float)L;
	if (mean < B2[0])
		(void)mbe_encoder_fit_floor(logm, L, mean, B2[0]);
	mbe_imbe4400_quantize_amplitudes(b0, bands, logm, prev, sync, bits);
	mbe_parms history = *prev;
	return mbe_decodeImbe4400Parms(bits, cur, &history) == 0 ? 0 : -1;
}

// --- tone frames ---------------------------------------------------------------

static int same_tone(float a1, float a2, float b1, float b2)
{
	return (fabsf(a1 - b1) < 0.5f && fabsf(a2 - b2) < 0.5f) || (fabsf(a1 - b2) < 0.5f && fabsf(a2 - b1) < 0.5f);
}

// the AMBE+2 Table 9 id of a tone, or -1
static int ambe2450_tone_id(float f1, float f2)
{
	for (int id = 5; id <= 163; id++)
	{
		float g1, g2;
		if (mbe_tone_lookup_freqs(id, &g1, &g2) && same_tone(f1, f2, g1, g2))
			return id;
	}
	return -1;
}

// the D-Star index of a tone, or -1
static int dstar_tone_index(float f1, float f2)
{
	for (int index = 5; index <= 147; index++)
	{
		float g1, g2;
		if (mbe_tone_lookup_dstar_freqs(index, &g1, &g2) && same_tone(f1, f2, g1, g2))
			return index;
	}
	return -1;
}

// as mbelib-neo's ambe2450_enc_pack_tone: u0 = 63 then AD(6..1), the tone id
// four times from bit 12, AD(0) at bit 44
static void ambe2450_pack_tone(int tone_id, int ad, char ambe_d[49])
{
	memset(ambe_d, 0, 49);
	for (int i = 0; i < 6; i++)
	{
		ambe_d[i] = 1;
		ambe_d[6 + i] = (char)((ad >> (6 - i)) & 1);
	}
	for (int i = 0; i < 32; i++)
		ambe_d[12 + i] = (char)((tone_id >> (7 - (i % 8))) & 1);
	ambe_d[44] = (char)(ad & 1);
}

// as mbelib-neo's ambe2400_enc_pack_tone: b0 126, the tone index and the volume
static void ambe2400_pack_tone(int index, int volume, char ambe_d[49])
{
	memset(ambe_d, 0, 49);
	for (int i = 0; i < 6; i++)
		ambe_d[i] = 1;
	mbe_ambe2400_set_tone_index(ambe_d, index);
	mbe_tone_dstar_set_volume(ambe_d, volume);
}

// a tone frame of the target, or 0 when it has none for these tones
static int tone_frame(const struct mbe_transcode_model *src, float gain_db, int dst, char *bits)
{
	const double db = (double)src->tone_db + (double)gain_db;
	if (dst == MBE_TRANSCODE_AMBE2450)
	{
		const int id = ambe2450_tone_id(src->tone_f1, src->tone_f2);
		if (id < 0)
			return 0;
		ambe2450_pack_tone(id, clamp_int(lround(127.0 + ((db - MBE_TONE_AMBE2450_DB_AT_AD127) / MBE_TONE_AMBE2450_DB_PER_STEP)), 0, 127), bits);
		return 1;
	}
	if (dst == MBE_TRANSCODE_AMBE2400)
	{
		const int index = dstar_tone_index(src->tone_f1, src->tone_f2);
		if (index < 0)
			return 0;
		ambe2400_pack_tone(index, clamp_int(lround(255.0 + ((db - MBE_TONE_DSTAR_DB_AT_VOL255) / MBE_TONE_DSTAR_DB_PER_STEP)), 1, 255), bits);
		return 1;
	}
	return 0;
}

// --- the encoder's side --------------------------------------------------------

int mbe_transcodeParms(const struct mbe_transcode_model *m, int src, int dst, float gain_db, mbe_parms *cur, mbe_parms *prev,
	int *sync, char bits[88])
{
	if (src < MBE_TRANSCODE_AMBE2400 || src > MBE_TRANSCODE_IMBE || dst < MBE_TRANSCODE_AMBE2400 || dst > MBE_TRANSCODE_IMBE || !isfinite(gain_db))
		return -1;
	gain_db = fmaxf(-GAIN_DB_MAX, fminf(GAIN_DB_MAX, gain_db));
	const int off = gain_db <= -GAIN_DB_MAX;	// the bottom of the range codes silence, tones too
	memset(bits, 0, 88);

	if (!off && m->kind == MBE_TRANSCODE_MODEL_TONE && tone_frame(m, gain_db, dst, bits))
		return 0;	// a tone frame leaves the prediction alone

	struct model mod;
	if (off)
		quiet_model(&mod);
	else if (m->kind == MBE_TRANSCODE_MODEL_VOICE)
		voice_model(m, src, gain_db * LOG2_PER_DB, &mod);
	else if (m->kind != MBE_TRANSCODE_MODEL_TONE || !tone_model(m->tone_f1, m->tone_f2, m->tone_db + gain_db, dst, &mod))
		quiet_model(&mod);

	int r;
	if (dst == MBE_TRANSCODE_AMBE2400)
		r = code_ambe2400(&mod, cur, prev, bits);
	else if (dst == MBE_TRANSCODE_AMBE2450)
		r = code_ambe2450(&mod, cur, prev, bits);
	else
		r = code_imbe(&mod, cur, prev, *sync, bits);

	if (r != 0 || !mbe_encoder_model_valid(cur))
	{
		*cur = *prev;	// no history the decoder could not hold either
		return -1;
	}
	mbe_moveMbeParms(cur, prev);
	if (dst == MBE_TRANSCODE_IMBE)
		*sync ^= 1;
	return 0;
}

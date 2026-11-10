// SPDX-License-Identifier: GPL-3.0-or-later
//
// mbelib-neo -- a tcd codec plugin: D-Star AMBE, DMR AMBE+2 and P25 IMBE from mbelib-neo
// (https://github.com/arancormonk/mbelib-neo), encoder and decoder in plain C,
// on any host
//
// mbelib-neo is GPL-2.0-or-later; its own patent notice says the use of the
// vocoders may implicate patents, so it stays a plugin.
//
// Frames as tcd has them:
//   D-Star  the 9 data bytes of a DV frame (mbe_encode/decodeDStarDVData)
//   DMR     the 72 bits of an AMBE+2 frame in air order, MSB first, which
//           DSD's dibit schedule (rW/rX/rY/rZ) maps to mbelib's 4x24 planes
//   P25     the 88 IMBE 4400 parameter bits u0..u7, MSB first in 11 bytes
//           (as OP25's encode_4400, tcd's own P25 vocoder); the FEC is not
//           in them, so decode_info reports no bit errors for P25, and it
//           mutes only after repeats of frames with an unusable pitch (b0)
//
// tcd.ini:  CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
//           CodecDStar = mbelib-neo      (to use it for D-Star although a device could)
//           CodecDMR   = mbelib-neo      (the same for DMR/YSF)
//           CodecP25   = mbelib-neo      (P25 instead of tcd's OP25, tcd 0.14.0 and later)
//
// Licensed under the GNU General Public License v3 or later

#include <cstdint>
#include <cstring>
#include <new>

extern "C" {
#include <mbelib-neo/mbelib.h>
}

#include "CodecPlugin.h"
#include "mbelib_neo_transcode.h"

#define PLUGIN_VERSION "1.2.1"

// transcode (mbelib_neo_transcode.c) uses mbelib-neo's private API of the
// version it is written for: another needs that file checked
static constexpr bool SameString(const char *a, const char *b)
{
	return *a == *b && (*a == '\0' || SameString(a + 1, b + 1));
}
static_assert(SameString(MBELIB_VERSION, MBE_TRANSCODE_MBELIB), "mbelib-neo is not the version mbelib_neo_transcode.c is written for (MBE_TRANSCODE_MBELIB)");

// DMR AMBE interleave schedule, from DSD (dmr_const.h, ISC license)
static const int rW[36] = { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2 };
static const int rX[36] = { 23, 10, 22, 9, 21, 8, 20, 7, 19, 6, 18, 5, 17, 4, 16, 3, 15, 2, 14, 1, 13, 0, 12, 10, 11, 9, 10, 8, 9, 7, 8, 6, 7, 5, 6, 4 };
static const int rY[36] = { 0, 2, 0, 2, 0, 2, 0, 2, 0, 3, 0, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3 };
static const int rZ[36] = { 5, 3, 4, 2, 3, 1, 2, 0, 1, 13, 0, 12, 22, 11, 21, 10, 20, 9, 19, 8, 18, 7, 17, 6, 16, 5, 15, 4, 14, 3, 13, 2, 12, 1, 11, 0 };

static void DmrToPlanes(const uint8_t *frame, char fr[4][24])
{
	memset(fr, 0, 4 * 24);
	for (int i = 0; i < 36; i++)
	{
		const int b = 2 * i;
		fr[rW[i]][rX[i]] = (frame[b >> 3] >> (7 - (b & 7))) & 1;
		fr[rY[i]][rZ[i]] = (frame[(b + 1) >> 3] >> (7 - ((b + 1) & 7))) & 1;
	}
}

static void PlanesToDmr(const char fr[4][24], uint8_t *frame)
{
	memset(frame, 0, 9);
	for (int i = 0; i < 36; i++)
	{
		const int b = 2 * i;
		if (fr[rW[i]][rX[i]] & 1)
			frame[b >> 3] |= uint8_t(0x80 >> (b & 7));
		if (fr[rY[i]][rZ[i]] & 1)
			frame[(b + 1) >> 3] |= uint8_t(0x80 >> ((b + 1) & 7));
	}
}

static void BitsToBytes(const char *bits, int n, uint8_t *bytes)
{
	memset(bytes, 0, size_t((n + 7) / 8));
	for (int i = 0; i < n; i++)
		if (bits[i] & 1)
			bytes[i >> 3] |= uint8_t(0x80 >> (i & 7));
}

static void BytesToBits(const uint8_t *bytes, int n, char *bits)
{
	for (int i = 0; i < n; i++)
		bits[i] = (bytes[i >> 3] >> (7 - (i & 7))) & 1;
}

// a stream: an encoder with its prediction, and a decoder with the model of
// the frame it decoded last, for transcode
struct SStream
{
	uint32_t codec;
	int vocoder;
	mbe_ambe2400_encoder *enc2400 = nullptr;
	mbe_ambe2450_encoder *enc2450 = nullptr;
	mbe_imbe4400_encoder *encImbe = nullptr;
	mbe_parms encCur, encPrev, encEnh;
	mbe_parms decCur, decPrev, decEnh;
	mbe_transcode_model model;
	int sync = 0;	// the IMBE synchronization bit transcode alternates
};

static int Vocoder(uint32_t codec)
{
	return codec == TCD_CODEC_DSTAR ? MBE_TRANSCODE_AMBE2400 : codec == TCD_CODEC_DMR ? MBE_TRANSCODE_AMBE2450 : MBE_TRANSCODE_IMBE;
}

static int Init(void) { return 0; }

static void *Open(uint32_t codec)
{
	if (codec != TCD_CODEC_DSTAR && codec != TCD_CODEC_DMR && codec != TCD_CODEC_P25)
		return nullptr;
	auto *st = new (std::nothrow) SStream;	// no exception through tcd's C interface
	if (!st)
		return nullptr;
	st->codec = codec;
	st->vocoder = Vocoder(codec);
	if (codec == TCD_CODEC_DSTAR)
		st->enc2400 = mbe_ambe2400EncoderAlloc();
	else if (codec == TCD_CODEC_DMR)
		st->enc2450 = mbe_ambe2450EncoderAlloc();
	else
		st->encImbe = mbe_imbe4400EncoderAlloc();
	if (!st->enc2400 && !st->enc2450 && !st->encImbe)
	{
		delete st;
		return nullptr;
	}
	mbe_initMbeParms(&st->encCur, &st->encPrev, &st->encEnh);
	mbe_initMbeParms(&st->decCur, &st->decPrev, &st->decEnh);
	mbe_transcodeForget(&st->model);
	return st;
}

// a new transmission: mbelib-neo's restart, encoder analysis and prediction
// and the decoder's prediction, repeat and muting state begin afresh
static void Start(void *state)
{
	auto *st = static_cast<SStream *>(state);
	mbe_ambe2400EncoderReset(st->enc2400);
	mbe_ambe2450EncoderReset(st->enc2450);
	mbe_imbe4400EncoderReset(st->encImbe);
	mbe_initMbeParms(&st->encCur, &st->encPrev, &st->encEnh);
	mbe_initMbeParms(&st->decCur, &st->decPrev, &st->decEnh);
	mbe_transcodeForget(&st->model);
	st->sync = 0;
}

static void Close(void *state)
{
	auto *st = static_cast<SStream *>(state);
	mbe_ambe2400EncoderFree(st->enc2400);
	mbe_ambe2450EncoderFree(st->enc2450);
	mbe_imbe4400EncoderFree(st->encImbe);
	delete st;
}

// on an error the frame is all zero bytes
// the parameter bits (88 for P25, 49 for AMBE) as tcd's frame of the codec,
// with the FEC of D-Star and DMR; on an error the frame is all zero bytes
static int PackFrame(uint32_t codec, const char *bits, uint8_t *frame)
{
	if (codec == TCD_CODEC_P25)
	{
		BitsToBytes(bits, 88, frame);
		return 0;
	}
	char fr[4][24];
	memset(frame, 0, 9);
	if (codec == TCD_CODEC_DSTAR)
	{
		if (mbe_encodeAmbe3600x2400Frame(bits, fr) < 0 || mbe_encodeDStarDVData(fr, frame) < 0)
		{
			memset(frame, 0, 9);
			return 1;
		}
		return 0;
	}
	if (mbe_encodeAmbe3600x2450Frame(bits, fr) < 0)
		return 1;
	PlanesToDmr(fr, frame);
	return 0;
}

static int Encode(void *state, const int16_t *pcm, uint8_t *frame)
{
	auto *st = static_cast<SStream *>(state);
	char bits[88] = {};	// 49 for AMBE
	int r;
	memset(frame, 0, st->codec == TCD_CODEC_P25 ? 11 : 9);
	if (st->codec == TCD_CODEC_P25)
		r = mbe_encodeImbe4400ParmsShort(st->encImbe, pcm, bits, &st->encCur, &st->encPrev);
	else if (st->codec == TCD_CODEC_DSTAR)
		r = mbe_encodeAmbe2400ParmsShort(st->enc2400, pcm, bits, &st->encCur, &st->encPrev);
	else
		r = mbe_encodeAmbe2450ParmsShort(st->enc2450, pcm, bits, &st->encCur, &st->encPrev);
	if (r < 0)
		return 1;
	mbe_moveMbeParms(&st->encCur, &st->encPrev);
	return PackFrame(st->codec, bits, frame);
}

static void Info(const mbe_process_result &result, tcd_frame_info *info)
{
	info->errors = result.total_errors;
	info->flags = 0;
	if (result.flags & MBE_PROCESS_FLAG_REPEAT)
		info->flags |= TCD_FRAME_REPEAT;
	if (result.flags & MBE_PROCESS_FLAG_MUTE)
		info->flags |= TCD_FRAME_MUTE;
	if (result.flags & MBE_PROCESS_FLAG_TONE)
		info->flags |= TCD_FRAME_TONE;
	if (result.flags & MBE_PROCESS_FLAG_SILENCE)
		info->flags |= TCD_FRAME_SILENCE;
}

// decode, and say how the frame came out
static int DecodeInfo(void *state, const uint8_t *frame, int16_t *pcm, tcd_frame_info *info)
{
	auto *st = static_cast<SStream *>(state);
	*info = tcd_frame_info{ 0, 0 };
	mbe_process_result result;
	mbe_initProcessResult(&result);
	int r;
	char bits[88] = {};	// the parameter bits, 49 of them for AMBE
	if (st->codec == TCD_CODEC_P25)
	{
		// the parameter bits only, without their FEC: no bit errors to count
		BytesToBits(frame, 88, bits);
		r = mbe_processImbe4400Data(pcm, &result, bits, &st->decCur, &st->decPrev, &st->decEnh);
	}
	else
	{
		char fr[4][24];
		if (st->codec == TCD_CODEC_DSTAR)
		{
			if (mbe_decodeDStarDVData(frame, fr) < 0)
			{
				mbe_transcodeForget(&st->model);
				return 1;
			}
			r = mbe_processAmbe3600x2400Frame(pcm, &result, fr, bits, &st->decCur, &st->decPrev, &st->decEnh);
		}
		else
		{
			DmrToPlanes(frame, fr);
			r = mbe_processAmbe3600x2450Frame(pcm, &result, fr, bits, &st->decCur, &st->decPrev, &st->decEnh);
		}
	}
	if (r < 0)
	{
		mbe_transcodeForget(&st->model);
		return 1;
	}
	mbe_transcodeRecord(&st->model, st->vocoder, &result, bits, &st->decCur, &st->decPrev);
	Info(result, info);
	return 0;
}

static int Decode(void *state, const uint8_t *frame, int16_t *pcm)
{
	tcd_frame_info info;
	return DecodeInfo(state, frame, pcm, &info);
}

// the frames tcd has: 9 bytes for D-Star and DMR, 11 for P25, 20 ms each
static int FrameFormat(uint32_t codec, uint32_t *bytes, uint32_t *samples)
{
	if (codec != TCD_CODEC_DSTAR && codec != TCD_CODEC_DMR && codec != TCD_CODEC_P25)
		return 1;
	*bytes = codec == TCD_CODEC_P25 ? 11 : 9;
	*samples = 160;
	return 0;
}

// API 4: the frame the decoder state decoded last, coded for the encoder
// state's codec from the vocoder model rather than from its audio; on an
// error the frame is all zero bytes, and tcd encodes the audio
static int Transcode(void *decoder, void *encoder, float gain_db, uint8_t *frame)
{
	auto *dec = static_cast<SStream *>(decoder);
	auto *st = static_cast<SStream *>(encoder);
	const uint32_t bytes = st->codec == TCD_CODEC_P25 ? 11 : 9;
	memset(frame, 0, bytes);
	if (dec->vocoder == st->vocoder)
		return 1;
	char bits[88];
	if (mbe_transcodeParms(&dec->model, dec->vocoder, st->vocoder, gain_db, &st->encCur, &st->encPrev, &st->sync, bits) != 0)
		return 1;
	return PackFrame(st->codec, bits, frame);
}

// API 4: tcd 0.20.0 and later load it
static const tcd_codec_plugin s_Plugin = {
	4,
	"mbelib-neo",
	"plugin " PLUGIN_VERSION ": D-Star AMBE, DMR AMBE+2 and P25 IMBE from mbelib-neo " MBELIB_VERSION,
	TCD_CODEC_DSTAR | TCD_CODEC_DMR | TCD_CODEC_P25,
	nullptr,	// no options
	Init,
	nullptr,	// nothing to shut down
	Open,
	Close,
	Encode,
	Decode,
	Start,
	nullptr,	// nothing to do at the end: the next start resets
	DecodeInfo,
	FrameFormat,
	Transcode,
};

// the plugin's only exported symbol (-fvisibility=hidden and the export list in the Makefile)
extern "C" __attribute__((visibility("default"))) const tcd_codec_plugin *tcd_codec_plugin(void)
{
	return &s_Plugin;
}

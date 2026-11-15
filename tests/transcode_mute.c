// SPDX-License-Identifier: GPL-3.0-or-later
//
// transcode_mute -- transcode after a mute (make check)
//
// After a mute (here the error rate's, on a valid frame), a lost frame below
// the repeat limit plays on: AMBE+2 from silence, the model the decoder kept
// underneath the mute; D-Star and P25 the muted frame as it was. It is
// audible on the source decoder, and transcode codes it into the target too
// instead of silence. Links mbelib_neo_transcode.o and mbelib-neo's static
// archive, as the plugin does, to set the decoder's error level.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <mbelib-neo/mbelib.h>

#include "mbelib_neo_transcode.h"

static double rms(const short *pcm)
{
	double e = 0;
	for (int i = 0; i < 160; i++)
		e += (double)pcm[i] * pcm[i];
	return sqrt(e / 160);
}

// a voiced frame: harmonics of 150 Hz
static void voiced_frame(int n, short *pcm)
{
	for (int i = 0; i < 160; i++)
	{
		const double t = (n * 160 + i) / 8000.0;
		double v = 0;
		for (int k = 1; k <= 12; k++)
			v += sin(2 * M_PI * 150.0 * k * t) / k;
		pcm[i] = (short)(3000 * v);
	}
}

static int fail(const char *name, const char *what)
{
	printf("FAIL %s: %s\n", name, what);
	return 1;
}

struct codec
{
	mbe_ambe2400_encoder *e2400;
	mbe_ambe2450_encoder *e2450;
	mbe_imbe4400_encoder *eimbe;
};

static int encode(struct codec *c, int vocoder, short *pcm, char *bits, mbe_parms *cur, mbe_parms *prev)
{
	if (vocoder == MBE_TRANSCODE_AMBE2400)
		return mbe_encodeAmbe2400ParmsShort(c->e2400, pcm, bits, cur, prev);
	if (vocoder == MBE_TRANSCODE_AMBE2450)
		return mbe_encodeAmbe2450ParmsShort(c->e2450, pcm, bits, cur, prev);
	return mbe_encodeImbe4400ParmsShort(c->eimbe, pcm, bits, cur, prev);
}

static int decode(int vocoder, short *pcm, mbe_process_result *result, const char *bits, mbe_parms *cur, mbe_parms *prev,
	mbe_parms *enh)
{
	mbe_initProcessResult(result);
	if (vocoder == MBE_TRANSCODE_AMBE2400)
		return mbe_processAmbe2400Data(pcm, result, bits, cur, prev, enh);
	if (vocoder == MBE_TRANSCODE_AMBE2450)
		return mbe_processAmbe2450Data(pcm, result, bits, cur, prev, enh);
	return mbe_processImbe4400Data(pcm, result, bits, cur, prev, enh);
}

// a lost frame, as tcd hands it on: AMBE+2's erasure (b0 120), D-Star's lost
// frame (b0 125, its last bit in ambe_d[48]) and P25's invalid pitch (b0 255)
static void lost_frame(int vocoder, char *bits)
{
	memset(bits, 0, 88);
	bits[0] = bits[1] = bits[2] = bits[3] = 1;
	if (vocoder == MBE_TRANSCODE_AMBE2400)
		bits[4] = bits[48] = 1;
	else if (vocoder == MBE_TRANSCODE_IMBE)
		bits[4] = bits[5] = bits[6] = bits[7] = 1;
}

// the source's error level on frame 20 mutes; the lost frames after it
// carry one below the mute: AMBE+2 and P25 follow the error rate, D-Star its
// error level (PSIl[0], decaying by 0.99 a frame from 342 up)
static void error_level(int vocoder, int n, mbe_parms *cur, mbe_parms *prev)
{
	if (vocoder == MBE_TRANSCODE_AMBE2400)
	{
		if (n >= 20)
			prev->PSIl[0] = n == 20 ? 400.0f : 300.0f;
	}
	else if (vocoder == MBE_TRANSCODE_AMBE2450)
	{
		// frame 20's error rate, 0.0969, mutes; a lost frame carries 0.05 on
		if (n == 20)
			prev->errorRate = 0.102f;
		else if (n > 20)
			cur->errorRate = 0.05f;
	}
	else if (n >= 20)
		prev->errorRate = n == 20 ? 0.2f : 0.05f;
}

static int run(struct codec *c, int src, int dst, const char *name)
{
	// the source encoder (ec, ep), the source decoder (dc, dp, de),
	// transcode's prediction for the target (xc, xp) and the target decoder
	// (oc, op, oe); unused is a scratch third set for the init of the first
	// and the third
	mbe_parms ec, ep, dc, dp, de, xc, xp, oc, op, oe, unused;
	mbe_initMbeParms(&ec, &ep, &unused);
	mbe_initMbeParms(&dc, &dp, &de);
	mbe_initMbeParms(&xc, &xp, &unused);
	mbe_initMbeParms(&oc, &op, &oe);
	struct mbe_transcode_model model;
	mbe_transcodeForget(&model);
	int sync = 0;
	short pcm[160], ref[160];
	char bits[88], lost[88], out[88];
	mbe_process_result result;
	unsigned flags[3] = { 0 };
	lost_frame(src, lost);
	for (int n = 0; n < 23; n++)
	{
		const char *frame = bits;
		if (n <= 20)
		{
			voiced_frame(n, pcm);
			if (encode(c, src, pcm, bits, &ec, &ep) < 0)
				return fail(name, "encode");
			mbe_moveMbeParms(&ec, &ep);
		}
		else
			frame = lost;
		error_level(src, n, &dc, &dp);
		if (decode(src, ref, &result, frame, &dc, &dp, &de) < 0)
			return fail(name, "decode the source");
		if (n >= 20)
			flags[n - 20] = result.flags;
		mbe_transcodeRecord(&model, src, &result, frame, &dc, &dp, &de);
		if (mbe_transcodeParms(&model, src, dst, 0.0f, &xc, &xp, &sync, out) != 0)
			return fail(name, "mbe_transcodeParms");
		if (decode(dst, pcm, &result, out, &oc, &op, &oe) < 0)
			return fail(name, "decode the target");
	}
	int failed = 0;
	if (!(flags[0] & MBE_PROCESS_FLAG_MUTE))
		failed = fail(name, "frame 20 does not mute");
	for (int i = 1; i < 3; i++)
		if (!(flags[i] & MBE_PROCESS_FLAG_REPEAT) || (flags[i] & MBE_PROCESS_FLAG_MUTE))
			failed = fail(name, "a lost frame after the mute does not repeat");
	// the last lost frame on the source decoder, the target decoder playing
	// what transcode coded
	printf("%s: mute, then lost: source %.0f, target %.0f\n", name, rms(ref), rms(pcm));
	if (rms(ref) <= 300.0)
		failed = fail(name, "the repeat after the mute is not heard");
	if (rms(pcm) <= 0.25 * rms(ref))
		failed = fail(name, "transcode codes silence for the repeat after the mute");
	return failed;
}

int main(void)
{
	struct codec c = { mbe_ambe2400EncoderAlloc(), mbe_ambe2450EncoderAlloc(), mbe_imbe4400EncoderAlloc() };
	int failed = 0;
	if (!c.e2400 || !c.e2450 || !c.eimbe)
		failed = fail("encoders", "alloc");
	else
	{
		failed |= run(&c, MBE_TRANSCODE_AMBE2450, MBE_TRANSCODE_AMBE2400, "dmr -> dstar");
		failed |= run(&c, MBE_TRANSCODE_AMBE2400, MBE_TRANSCODE_AMBE2450, "dstar -> dmr");
		failed |= run(&c, MBE_TRANSCODE_IMBE, MBE_TRANSCODE_AMBE2400, "p25 -> dstar");
	}
	mbe_ambe2400EncoderFree(c.e2400);
	mbe_ambe2450EncoderFree(c.e2450);
	mbe_imbe4400EncoderFree(c.eimbe);
	return failed ? 1 : 0;
}

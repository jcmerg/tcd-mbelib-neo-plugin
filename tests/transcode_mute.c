// SPDX-License-Identifier: GPL-3.0-or-later
//
// transcode_mute -- transcode after a DMR mute (make check)
//
// After an AMBE+2 mute (here the error rate's, on a valid frame), a lost frame
// below the repeat limit plays the model the decoder kept underneath the mute,
// from silence: audible on the DMR decoder, and transcode codes it into D-Star
// too instead of silence. Links mbelib_neo_transcode.o and mbelib-neo's static
// archive, as the plugin does, to set the decoder's error rate.

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

static int fail(const char *what)
{
	printf("FAIL %s\n", what);
	return 1;
}

int main(void)
{
	mbe_ambe2450_encoder *enc = mbe_ambe2450EncoderAlloc();
	// the DMR encoder (ec, ep), the DMR decoder (dc, dp, de), transcode's
	// D-Star prediction (xc, xp) and the D-Star decoder (oc, op, oe); unused
	// is a scratch third set for the init of the first and the third
	mbe_parms ec, ep, dc, dp, de, xc, xp, oc, op, oe, unused;
	mbe_initMbeParms(&ec, &ep, &unused);
	mbe_initMbeParms(&dc, &dp, &de);
	mbe_initMbeParms(&xc, &xp, &unused);
	mbe_initMbeParms(&oc, &op, &oe);
	struct mbe_transcode_model model;
	mbe_transcodeForget(&model);
	int sync = 0;
	short pcm[160], ref[160];
	char bits[49], dstar[88];
	mbe_process_result result;
	if (!enc)
		return fail("mbe_ambe2450EncoderAlloc");
	// a lost frame, as tcd hands it on: an erasure (b0 120)
	char lost[49] = { 0 };
	lost[0] = lost[1] = lost[2] = lost[3] = 1;
	unsigned flags[3] = { 0 };
	for (int n = 0; n < 23; n++)
	{
		const char *frame = bits;
		if (n <= 20)
		{
			voiced_frame(n, pcm);
			if (mbe_encodeAmbe2450ParmsShort(enc, pcm, bits, &ec, &ep) < 0)
				return fail("mbe_encodeAmbe2450ParmsShort");
			mbe_moveMbeParms(&ec, &ep);
		}
		else
			frame = lost;
		// frame 20's error rate, 0.0969, mutes; a lost frame carries 0.05 on, below it
		if (n == 20)
			dp.errorRate = 0.102f;
		else if (n > 20)
			dc.errorRate = 0.05f;
		mbe_initProcessResult(&result);
		if (mbe_processAmbe2450Data(ref, &result, frame, &dc, &dp, &de) < 0)
			return fail("mbe_processAmbe2450Data");
		if (n >= 20)
			flags[n - 20] = result.flags;
		mbe_transcodeRecord(&model, MBE_TRANSCODE_AMBE2450, &result, frame, &dc, &dp, &de);
		if (mbe_transcodeParms(&model, MBE_TRANSCODE_AMBE2450, MBE_TRANSCODE_AMBE2400, 0.0f, &xc, &xp, &sync, dstar) != 0)
			return fail("mbe_transcodeParms");
		mbe_initProcessResult(&result);
		if (mbe_processAmbe2400Data(pcm, &result, dstar, &oc, &op, &oe) < 0)
			return fail("mbe_processAmbe2400Data");
	}
	int failed = 0;
	if (!(flags[0] & MBE_PROCESS_FLAG_MUTE))
		failed = fail("frame 20 does not mute");
	for (int i = 1; i < 3; i++)
		if (!(flags[i] & MBE_PROCESS_FLAG_REPEAT) || (flags[i] & MBE_PROCESS_FLAG_MUTE))
			failed = fail("a lost frame after the mute does not repeat");
	// the last lost frame on the DMR decoder, the D-Star decoder playing what transcode coded
	printf("dmr mute, then lost -> dstar: source %.0f, target %.0f\n", rms(ref), rms(pcm));
	if (rms(ref) <= 300.0)
		failed = fail("the repeat after the mute is not heard");
	if (rms(pcm) <= 0.25 * rms(ref))
		failed = fail("transcode codes silence for the repeat after the mute");
	mbe_ambe2450EncoderFree(enc);
	return failed ? 1 : 0;
}

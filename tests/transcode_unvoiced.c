// SPDX-License-Identifier: GPL-3.0-or-later
//
// transcode_unvoiced -- the pitch code of a transcoded frame (make check)
//
// A frame with no voiced band goes out at one fixed pitch code, as
// mbelib-neo's encoders send it: D-Star b0 123 and AMBE+2 b0 119 (56
// harmonics each), P25 b0 127 (37 harmonics). A voiced frame keeps the
// source's pitch. Each target frame is decoded again by mbelib-neo's
// decoder, whose model transcode's prediction history must equal (D-Star
// also in the frame mark the pitch code sets).

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <mbelib-neo/mbelib.h>

#include "mbelib_neo_transcode.h"

static const char *names[3] = { "dstar", "dmr", "p25" };

// a model at 200 Hz, 18 harmonics, every one voiced or none
static void model_200hz(struct mbe_transcode_model *m, int voiced)
{
	mbe_transcodeForget(m);
	m->kind = MBE_TRANSCODE_MODEL_VOICE;
	m->w0 = 2.0f * (float)M_PI * 200.0f / 8000.0f;
	m->L = 18;
	for (int l = 1; l <= m->L; l++)
	{
		m->Vl[l] = voiced;
		m->log2Ml[l] = 5.0f - 0.1f * (float)l;
	}
}

static int decode(int dst, const char *bits, mbe_parms *cur, mbe_parms *prev)
{
	if (dst == MBE_TRANSCODE_AMBE2400)
		return mbe_decodeAmbe2400Parms(bits, cur, prev);
	if (dst == MBE_TRANSCODE_AMBE2450)
		return mbe_decodeAmbe2450Parms(bits, cur, prev) == MBE_AMBE2450_FRAME_VOICE ? 0 : -1;
	return mbe_decodeImbe4400Parms(bits, cur, prev);
}

static int check(int dst, int voiced)
{
	static const int fixed_L[3] = { 56, 56, 37 };
	const int src = (dst == MBE_TRANSCODE_AMBE2400) ? MBE_TRANSCODE_AMBE2450 : MBE_TRANSCODE_AMBE2400;
	mbe_parms xc, xp, dc, dp, unused;
	mbe_initMbeParms(&xc, &xp, &unused);
	mbe_initMbeParms(&dc, &dp, &unused);
	struct mbe_transcode_model m;
	model_200hz(&m, voiced);
	int sync = 0;
	char bits[88];
	// a few frames, so the prediction has a history to follow
	for (int n = 0; n < 4; n++)
	{
		if (mbe_transcodeParms(&m, src, dst, 0.0f, &xc, &xp, &sync, bits) != 0)
		{
			printf("FAIL %s: transcode\n", names[dst]);
			return 1;
		}
		if (decode(dst, bits, &dc, &dp) != 0)
		{
			printf("FAIL %s: decode\n", names[dst]);
			return 1;
		}
		int any_voiced = 0;
		for (int l = 1; l <= dc.L; l++)
			any_voiced |= dc.Vl[l] != 0;
		// transcode's history (moved into xp) is the decoder's model
		if (xp.L != dc.L || fabsf(xp.w0 - dc.w0) > 1e-6f || xp.PHIl[0] != dc.PHIl[0])
		{
			printf("FAIL %s: transcode's model (L %d, w0 %.5f, mark %.0f) is not the decoder's (L %d, w0 %.5f, mark %.0f)\n",
				names[dst], xp.L, (double)xp.w0, (double)xp.PHIl[0], dc.L, (double)dc.w0, (double)dc.PHIl[0]);
			return 1;
		}
		if (!voiced && (any_voiced || dc.L != fixed_L[dst]))
		{
			printf("FAIL %s: unvoiced frame decodes with L %d%s, not at the fixed pitch code (L %d)\n", names[dst], dc.L,
				any_voiced ? " and a voiced band" : "", fixed_L[dst]);
			return 1;
		}
		if (voiced && (!any_voiced || dc.L == fixed_L[dst]))
		{
			printf("FAIL %s: voiced 200 Hz frame decodes with L %d%s\n", names[dst], dc.L, any_voiced ? "" : ", no voiced band");
			return 1;
		}
		mbe_moveMbeParms(&dc, &dp);
	}
	printf("%s: %s frames at L %d\n", names[dst], voiced ? "voiced" : "unvoiced", dc.L);
	return 0;
}

int main(void)
{
	int failed = 0;
	for (int dst = MBE_TRANSCODE_AMBE2400; dst <= MBE_TRANSCODE_IMBE; dst++)
	{
		failed |= check(dst, 0);
		failed |= check(dst, 1);
	}
	return failed;
}

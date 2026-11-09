// SPDX-License-Identifier: GPL-3.0-or-later
//
// transcode for the mbelib-neo plugin: the frame a decoder last decoded, coded
// again by the encoder of another MBE codec from the vocoder model, without
// synthesis and analysis (tcd codec API 4)
//
// Built against mbelib-neo's private headers and archive symbols of exactly
// one version, MBE_TRANSCODE_MBELIB: the Makefile checks the source tree,
// mbelib_neo_plugin.cpp the installed library. Another version needs
// mbelib_neo_transcode.c checked against its src/internal first.

#pragma once

#include <mbelib-neo/mbelib.h>

#ifdef __cplusplus
extern "C" {
#endif

// the mbelib-neo version mbelib_neo_transcode.c is written for
#define MBE_TRANSCODE_MBELIB "2.6.7"

// the three vocoders
#define MBE_TRANSCODE_AMBE2400 0	// D-Star
#define MBE_TRANSCODE_AMBE2450 1	// DMR/YSF/NXDN
#define MBE_TRANSCODE_IMBE     2	// P25

// what a decoded frame leaves for the transcoder
struct mbe_transcode_model
{
	int kind;	// MBE_TRANSCODE_MODEL_* in mbelib_neo_transcode.c, 0: mute
	float w0;
	int L;
	int Vl[57];
	float log2Ml[57];
	float tone_f1, tone_f2, tone_db;
	int tone_repeats;	// a repeat plays the tone on (D-Star DTMF and call progress)
};

// a frame that could not be decoded: transcode codes silence for it
void mbe_transcodeForget(struct mbe_transcode_model *m);

// after mbelib-neo's process function: its result, the parameter bits it
// decoded (49 or 88) and the decoder's cur and prev
void mbe_transcodeRecord(struct mbe_transcode_model *m, int vocoder, const mbe_process_result *result, const char *bits,
	const mbe_parms *cur, const mbe_parms *prev);

// the model coded for vocoder dst into bits (49 or 88), the level changed by
// gain_db; cur and prev are the encoder's prediction history, sync the IMBE
// synchronization bit it alternates. 0 = ok, then cur has moved to prev; on
// an error the history stays as it was.
int mbe_transcodeParms(const struct mbe_transcode_model *m, int src, int dst, float gain_db, mbe_parms *cur, mbe_parms *prev,
	int *sync, char bits[88]);

#ifdef __cplusplus
}
#endif

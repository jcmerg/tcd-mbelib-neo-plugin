// mbelib-neo -- a tcd codec plugin: D-Star AMBE and DMR AMBE+2 from mbelib-neo
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
//
// tcd.ini:  CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
//           CodecDStar = mbelib-neo      (to use it for D-Star although a device could)
//           CodecDMR   = mbelib-neo      (the same for DMR/YSF)
//
// Licensed under the GNU General Public License v3 or later

#include <cstdint>
#include <cstring>

extern "C" {
#include <mbelib-neo/mbelib.h>
}

#include "CodecPlugin.h"

#define PLUGIN_VERSION "1.0.2"

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

// a stream: an encoder with its prediction, and a decoder
struct SStream
{
	uint32_t codec;
	mbe_ambe2400_encoder *enc2400 = nullptr;
	mbe_ambe2450_encoder *enc2450 = nullptr;
	mbe_parms encCur, encPrev, encEnh;
	mbe_parms decCur, decPrev, decEnh;
};

static int Init(void) { return 0; }

static void *Open(uint32_t codec)
{
	if (codec != TCD_CODEC_DSTAR && codec != TCD_CODEC_DMR)
		return nullptr;
	auto *st = new SStream;
	st->codec = codec;
	if (codec == TCD_CODEC_DSTAR)
		st->enc2400 = mbe_ambe2400EncoderAlloc();
	else
		st->enc2450 = mbe_ambe2450EncoderAlloc();
	if (!st->enc2400 && !st->enc2450)
	{
		delete st;
		return nullptr;
	}
	mbe_initMbeParms(&st->encCur, &st->encPrev, &st->encEnh);
	mbe_initMbeParms(&st->decCur, &st->decPrev, &st->decEnh);
	return st;
}

// a new transmission: mbelib-neo's restart, encoder analysis and prediction
// and the decoder's prediction, repeat and muting state begin afresh
static void Start(void *state)
{
	auto *st = static_cast<SStream *>(state);
	mbe_ambe2400EncoderReset(st->enc2400);
	mbe_ambe2450EncoderReset(st->enc2450);
	mbe_initMbeParms(&st->encCur, &st->encPrev, &st->encEnh);
	mbe_initMbeParms(&st->decCur, &st->decPrev, &st->decEnh);
}

static void Close(void *state)
{
	auto *st = static_cast<SStream *>(state);
	mbe_ambe2400EncoderFree(st->enc2400);
	mbe_ambe2450EncoderFree(st->enc2450);
	delete st;
}

static int Encode(void *state, const int16_t *pcm, uint8_t *frame)
{
	auto *st = static_cast<SStream *>(state);
	char ambe_d[49] = {}, fr[4][24];
	if (st->codec == TCD_CODEC_DSTAR)
	{
		if (mbe_encodeAmbe2400ParmsShort(st->enc2400, pcm, ambe_d, &st->encCur, &st->encPrev) < 0)
			return 1;
		mbe_moveMbeParms(&st->encCur, &st->encPrev);
		if (mbe_encodeAmbe3600x2400Frame(ambe_d, fr) < 0 || mbe_encodeDStarDVData(fr, frame) < 0)
			return 1;
	}
	else
	{
		if (mbe_encodeAmbe2450ParmsShort(st->enc2450, pcm, ambe_d, &st->encCur, &st->encPrev) < 0)
			return 1;
		mbe_moveMbeParms(&st->encCur, &st->encPrev);
		if (mbe_encodeAmbe3600x2450Frame(ambe_d, fr) < 0)
			return 1;
		PlanesToDmr(fr, frame);
	}
	return 0;
}

static int Decode(void *state, const uint8_t *frame, int16_t *pcm)
{
	auto *st = static_cast<SStream *>(state);
	char ambe_d[49] = {}, fr[4][24];
	mbe_process_result result;
	mbe_initProcessResult(&result);
	int r;
	if (st->codec == TCD_CODEC_DSTAR)
	{
		if (mbe_decodeDStarDVData(frame, fr) < 0)
			return 1;
		r = mbe_processAmbe3600x2400Frame(pcm, &result, fr, ambe_d, &st->decCur, &st->decPrev, &st->decEnh);
	}
	else
	{
		DmrToPlanes(frame, fr);
		r = mbe_processAmbe3600x2450Frame(pcm, &result, fr, ambe_d, &st->decCur, &st->decPrev, &st->decEnh);
	}
	return r < 0 ? 1 : 0;
}

static const tcd_codec_plugin s_Plugin = {
	TCD_CODEC_API,
	"mbelib-neo",
	"plugin " PLUGIN_VERSION ": D-Star AMBE and DMR AMBE+2 from mbelib-neo " MBELIB_VERSION,
	TCD_CODEC_DSTAR | TCD_CODEC_DMR,
	nullptr,	// no options
	Init,
	nullptr,	// nothing to shut down
	Open,
	Close,
	Encode,
	Decode,
	Start,
	nullptr,	// nothing to do at the end: the next start resets
};

extern "C" const tcd_codec_plugin *tcd_codec_plugin(void)
{
	return &s_Plugin;
}

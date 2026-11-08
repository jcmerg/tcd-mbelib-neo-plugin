// SPDX-License-Identifier: GPL-3.0-or-later
//
// smoke -- loads the plugin as tcd does and runs each codec (make check)
//
//   tests/smoke ./mbelib_neo_codec.so
//
// It checks what tcd refuses a plugin for (api, name, the frame formats of its
// table), that only tcd_codec_plugin is exported, that each codec encodes and
// decodes a tone to something audible without bit errors, that a flipped bit
// is counted for D-Star and DMR (not for P25, whose frames have no FEC) and
// that a garbled DMR frame comes out as a repeat.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <dlfcn.h>

#include "CodecPlugin.h"

static int s_Failed = 0;

static void Check(bool ok, const char *what, const char *codec)
{
	if (!ok)
	{
		std::fprintf(stderr, "FAIL %s: %s\n", codec, what);
		s_Failed++;
	}
}

struct SCodec
{
	uint32_t codec;
	const char *name;
	uint32_t bytes;
	uint32_t samples;
};

// tcd's table of the frame formats (CodecPlugins.cpp), the plugin's codecs
static const SCodec s_Codecs[] = {
	{ TCD_CODEC_DSTAR, "dstar", 9, 160 },
	{ TCD_CODEC_DMR, "dmr", 9, 160 },
	{ TCD_CODEC_P25, "p25", 11, 160 },
};

static void Roundtrip(const tcd_codec_plugin *p, const SCodec &c)
{
	uint32_t bytes = 0, samples = 0;
	Check(p->frame_format(c.codec, &bytes, &samples) == 0, "frame_format", c.name);
	Check(bytes == c.bytes && samples == c.samples, "the frame format is not tcd's", c.name);
	if (bytes != c.bytes || samples != c.samples)
		return;

	void *enc = p->open(c.codec);
	void *dec = p->open(c.codec);
	Check(enc && dec, "open", c.name);
	if (!enc || !dec)
		return;
	p->start(enc);
	p->start(dec);

	// 440 Hz: after the encoder's delay the decoder gives it back
	std::vector<int16_t> pcm(samples), out(samples);
	std::vector<uint8_t> frame(bytes);
	double energy = 0;
	for (int n = 0, t = 0; n < 20; n++)
	{
		for (auto &s : pcm)
			s = int16_t(8000 * std::sin(2 * M_PI * 440 * t++ / 8000));
		Check(p->encode(enc, pcm.data(), frame.data()) == 0, "encode", c.name);
		tcd_frame_info info{ -1, 0xffffffffu };
		Check(p->decode_info(dec, frame.data(), out.data(), &info) == 0, "decode_info", c.name);
		Check(info.errors == 0, "bit errors in an encoded frame", c.name);
		Check((info.flags & (TCD_FRAME_REPEAT | TCD_FRAME_MUTE)) == 0, "an encoded frame repeated or muted", c.name);
		if (n >= 10)
			for (auto s : out)
				energy += double(s) * s;
	}
	Check(energy / (10.0 * samples) > 1e4, "the decoded tone is (almost) silent", c.name);
	Check(p->decode(dec, frame.data(), out.data()) == 0, "decode", c.name);

	// one flipped bit: the FEC of D-Star and DMR corrects and counts it, a P25
	// frame has the parameter bits only
	frame[0] ^= 0x80;
	tcd_frame_info info{ 0, 0 };
	Check(p->decode_info(dec, frame.data(), out.data(), &info) == 0, "decode_info of a frame with a bit error", c.name);
	if (c.codec == TCD_CODEC_P25)
		Check(info.errors == 0, "bit errors counted without FEC", c.name);
	else
		Check(info.errors > 0, "a flipped bit is not counted", c.name);

	// too many bit errors for the FEC: the decoder repeats the last frame
	if (c.codec == TCD_CODEC_DMR)
	{
		std::memset(frame.data(), 0xff, bytes);
		info = tcd_frame_info{ 0, 0 };
		Check(p->decode_info(dec, frame.data(), out.data(), &info) == 0, "decode_info of a garbled frame", c.name);
		Check(info.errors > 0 && (info.flags & TCD_FRAME_REPEAT), "a garbled frame is no repeat", c.name);
	}

	if (p->stop)
		p->stop(enc);
	if (p->stop)
		p->stop(dec);
	p->close(enc);
	p->close(dec);
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::fprintf(stderr, "usage: %s <plugin.so>\n", argv[0]);
		return 2;
	}
	void *so = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	if (!so)
	{
		std::fprintf(stderr, "FAIL dlopen: %s\n", dlerror());
		return 1;
	}
	auto get = reinterpret_cast<tcd_codec_plugin_func>(dlsym(so, "tcd_codec_plugin"));
	const tcd_codec_plugin *p = get ? get() : nullptr;
	if (!p)
	{
		std::fprintf(stderr, "FAIL tcd_codec_plugin: %s\n", get ? "NULL" : dlerror());
		return 1;
	}
	// mbelib-neo's own symbols stay inside the plugin
	Check(!dlsym(so, "mbe_ambe2400EncoderAlloc") && !dlsym(so, "mbe_initMbeParms"), "mbelib-neo's symbols are exported", "plugin");

	Check(p->api >= TCD_CODEC_API_MIN && p->api <= TCD_CODEC_API, "api", "plugin");
	Check(p->name && std::strcmp(p->name, "mbelib-neo") == 0, "name", "plugin");
	Check(p->codecs == (TCD_CODEC_DSTAR | TCD_CODEC_DMR | TCD_CODEC_P25), "codecs", "plugin");
	Check(p->init && p->open && p->close && p->encode && p->decode, "a required function is NULL", "plugin");
	Check(p->start && p->decode_info && p->frame_format, "start, decode_info or frame_format is NULL", "plugin");
	if (s_Failed)
		return 1;
	Check(p->init() == 0, "init", "plugin");

	for (const auto &c : s_Codecs)
		Roundtrip(p, c);
	uint32_t bytes, samples;
	Check(p->open(0x80000000u) == nullptr, "open of an unknown codec", "plugin");
	Check(p->frame_format(0x80000000u, &bytes, &samples) != 0, "frame_format of an unknown codec", "plugin");

	if (p->shutdown)
		p->shutdown();
	if (s_Failed)
	{
		std::fprintf(stderr, "%d check(s) failed\n", s_Failed);
		return 1;
	}
	std::printf("%s: %s, all checks passed\n", argv[1], p->description);
	return 0;
}

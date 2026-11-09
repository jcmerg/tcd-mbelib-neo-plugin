// SPDX-License-Identifier: GPL-3.0-or-later
//
// smoke -- loads the plugin as tcd does and runs each codec (make check)
//
//   tests/smoke ./mbelib_neo_codec.so
//
// It checks what tcd refuses a plugin for (api, name, the frame formats of its
// table), that only tcd_codec_plugin is exported, that each codec encodes and
// decodes a tone to something audible without bit errors, that a flipped bit
// is counted for D-Star and DMR (not for P25, whose frames have no FEC), that
// a garbled DMR frame comes out as a repeat, and that transcode codes each of
// the three into each other at the level of the audio path, and -60 dB as
// silence.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
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

// a vowel: harmonics of 180 Hz under three formants
static void Vowel(std::vector<int16_t> &pcm, int &t)
{
	for (auto &s : pcm)
	{
		double v = 0;
		for (int k = 1; k * 180.0 < 3800; k++)
		{
			const double f = k * 180.0;
			const double env = 1 / (1 + std::pow((f - 600) / 250, 2)) + 0.5 / (1 + std::pow((f - 1400) / 300, 2))
				+ 0.25 / (1 + std::pow((f - 2600) / 400, 2));
			v += env * std::sin(2 * M_PI * f * t / 8000 + k * k * 0.3);
		}
		t++;
		s = int16_t(3000 * v);
	}
}

// the energy per sample of the vowel coded with codec a, decoded, coded with
// codec b (transcode, or encode of the decoded audio as tcd's audio path) and
// decoded, frames 10..29
static double Through(const tcd_codec_plugin *p, const SCodec &a, const SCodec &b, bool transcode, float gain_db)
{
	const std::string name = std::string(a.name) + "->" + b.name;
	void *enc = p->open(a.codec), *dec = p->open(a.codec), *tc = p->open(b.codec), *out = p->open(b.codec);
	Check(enc && dec && tc && out, "open", name.c_str());
	double energy = 0;
	if (enc && dec && tc && out)
	{
		for (void *s : { enc, dec, tc, out })
			p->start(s);
		std::vector<int16_t> pcm(160);
		std::vector<uint8_t> fa(a.bytes), fb(b.bytes);
		for (int n = 0, t = 0; n < 30; n++)
		{
			Vowel(pcm, t);
			Check(p->encode(enc, pcm.data(), fa.data()) == 0, "encode", name.c_str());
			Check(p->decode(dec, fa.data(), pcm.data()) == 0, "decode", name.c_str());
			if (transcode)
				Check(p->transcode(dec, tc, gain_db, fb.data()) == 0, "transcode", name.c_str());
			else
				Check(p->encode(tc, pcm.data(), fb.data()) == 0, "encode", name.c_str());
			tcd_frame_info info{ -1, 0xffffffffu };
			Check(p->decode_info(out, fb.data(), pcm.data(), &info) == 0, "decode_info", name.c_str());
			Check(info.errors == 0, "bit errors in a transcoded frame", name.c_str());
			Check((info.flags & (TCD_FRAME_REPEAT | TCD_FRAME_MUTE)) == 0, "a transcoded frame repeated or muted", name.c_str());
			if (n >= 10)
				for (auto s : pcm)
					energy += double(s) * s;
		}
	}
	for (void *s : { enc, dec, tc, out })
		if (s)
			p->close(s);
	return energy / (20.0 * 160);
}

static void Transcode(const tcd_codec_plugin *p)
{
	for (const auto &a : s_Codecs)
	{
		for (const auto &b : s_Codecs)
		{
			if (a.codec == b.codec)
				continue;
			const std::string name = std::string(a.name) + "->" + b.name;
			const double audio = Through(p, a, b, false, 0.0f);
			const double db = 10 * std::log10(Through(p, a, b, true, 0.0f) / audio);
			std::printf("transcode %-10s %+5.1f dB against the audio path\n", name.c_str(), db);
			Check(std::fabs(db) < 3.0, "the transcoded level is 3 dB or more off the audio path's", name.c_str());
			Check(Through(p, a, b, true, -60.0f) < 1e-3 * audio, "-60 dB is not silent", name.c_str());
		}
		// the same codec is not transcoded
		void *dec = p->open(a.codec), *enc = p->open(a.codec);
		std::vector<uint8_t> f(a.bytes);
		if (dec && enc)
			Check(p->transcode(dec, enc, 0.0f, f.data()) != 0, "transcode into its own codec", a.name);
		for (void *s : { dec, enc })
			if (s)
				p->close(s);
	}
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
	Check(p->api == 4 && p->transcode, "no transcode (api 4)", "plugin");
	if (s_Failed)
		return 1;
	Check(p->init() == 0, "init", "plugin");

	for (const auto &c : s_Codecs)
		Roundtrip(p, c);
	Transcode(p);
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

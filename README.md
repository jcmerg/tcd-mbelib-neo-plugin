# tcd-mbelib-neo-plugin

[English](README.md) | [Deutsch](README.de.md)

A codec plugin for **tcd**, the transcoder of [urfd-ng](https://github.com/jcmerg/urfd-ng): D-Star (AMBE 3600x2400) and DMR/YSF/NXDN (AMBE+2 3600x2450) in software, encoder and decoder, from [mbelib-neo](https://github.com/arancormonk/mbelib-neo). With it, tcd transcodes without DVSI hardware, or more modules than the AMBE sticks have channels. It also does P25 (IMBE 4400), as a choice next to tcd's own OP25 vocoder; urfd-ng 4.31.0 and later hand YSF VW (Voice Wide, the same IMBE) to tcd as P25 too.

Since 1.2.0 it also transcodes D-Star, DMR/YSF and P25 into each other directly (`transcode`, tcd 0.20.0 and later, `Transcode = direct`): the outgoing frame is made from the vocoder parameters the decoder read from the incoming one, without synthesis and a second analysis.

## Patent notice

**Read this before you build or use it.** Software implementations of AMBE and AMBE+2 may implicate patents. mbelib-neo's own [patent research record](https://github.com/arancormonk/mbelib-neo/blob/main/legal/patents.md) lists US 8,359,197 (AMBE+2 half rate) as active, with a projected expiry of 2028-05-20, and further open records. In Europe its counterparts (EP 1 465 158, EP 1 748 425) and DVSI's older patents ran out after 20 years, by 2024-03-26 at the latest; a search of DVSI's European patents and applications (2026-10-28) found no granted patent in force whose method this plugin uses, but applications are pending and can still be granted. No patent license is given or implied by this plugin, by mbelib-neo or by the GPL. Whether to build and use it, and where, is your decision; this is no legal advice. This plugin is therefore not part of urfd-ng: its setup and builds take it only when you clone it into `tcd/plugins/` yourself, and it is off unless you load it.

On direct transcoding (`transcode`, since 1.2.0): DVSI patented converting between MBE vocoders at the parameter level, as "Voice transcoder" (US 7,634,399, with its divisional US 7,957,963) and in the "Interoperable vocoder" family (US 7,970,606, US 8,315,860, EP 1 420 390 with DE 603 16 396, CA 2,447,735, JP 4,166,673). According to Google Patents (retrieved 2026-11-09) all of them have expired, US 7,634,399 on 2025-11-07 and EP 1 420 390 on 2023-11-07. Newer DVSI applications the search found are, by their titles, about other things (tone detection, error correction). `transcode` uses the AMBE+2 quantizer, for which US 8,359,197 above still applies. This too is research, not a complete search and not legal advice; Google Patents' status is not an official register.

## Build

Needs a C/C++ compiler, `make`, `cmake` (3.13 or later), `tar` and `curl` or `wget` (Debian, Ubuntu, Raspberry Pi OS; x86_64, arm64, armhf; macOS builds it too).

```bash
git clone https://github.com/jcmerg/tcd-mbelib-neo-plugin.git
cd tcd-mbelib-neo-plugin
./build-mbelib-neo.sh        # downloads mbelib-neo 2.6.7 (checked against its SHA-256) and builds it here
make
make check                   # loads the plugin as tcd does and runs each codec once
sudo make install            # /usr/local/lib/tcd/mbelib_neo_codec.so
```

`CFLAGS`, `CXXFLAGS`, `CPPFLAGS` and `LDFLAGS` from the environment are taken (the default is `-O2`), `make WERROR=1` makes warnings errors, `make DESTDIR=… install` installs into a staging directory. The plugin exports `tcd_codec_plugin` only; mbelib-neo's symbols stay inside it.

Or clone this repository into `tcd/plugins/mbelib-neo` of the urfd-ng checkout: the tcd Docker image and `tcd/build.sh` build it with tcd and install it, and `setup.sh update` pulls it (see "Codec plugins" and "tcd in Docker" in [tcd/README.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.md#codec-plugins)).

To test it, urfd-ng has `tcd/tools/` (`make` there): `./codectest /usr/local/lib/tcd/mbelib_neo_codec.so dmr|dstar|p25 in.raw out.frames out.raw` encodes and decodes 8 kHz s16le PCM and prints the time per frame; with two codecs (`dstar:dmr`, any two of `dstar`, `dmr`, `p25`) it makes the second one's frames with `transcode`, as tcd's direct path; `dvsitest` compares with a DV3003.

mbelib-neo is not part of this repository: `build-mbelib-neo.sh` fetches its release source archive and the plugin links it statically. Only the D-Star, AMBE+2 and IMBE parts end up in the plugin. On 64-bit hosts mbelib-neo uses its SIMD routines by default; on 32-bit ARM (armhf, e.g. Raspberry Pi OS) the script switches its NEON routines on when the CPU has NEON (Raspberry Pi 2 and later). The binaries stay portable (`MBELIB_ENABLE_NATIVE=OFF`).

### Updating mbelib-neo

The plugin is pinned to exactly one mbelib-neo version, so a build is the same everywhere: `build-mbelib-neo.sh` fetches it and checks its SHA-256. `transcode` (`mbelib_neo_transcode.c`) uses mbelib-neo's private headers and quantizers for it (`src/internal`, in the static archive), which mbelib-neo does not promise to keep stable; so the plugin builds only with the version that file is written for (`MBE_TRANSCODE_MBELIB` in `mbelib_neo_transcode.h`). The `Makefile` checks it on the source tree, `mbelib_neo_plugin.cpp` on the installed library; another version stops the build rather than sending wrong frames.

For a newer release of [mbelib-neo](https://github.com/arancormonk/mbelib-neo/releases), after reading its release notes:

1. Get the sum:
   ```bash
   VER=2.7.0   # the new version
   curl -fsSL https://github.com/arancormonk/mbelib-neo/releases/download/v$VER/mbelib-neo-$VER-source.tar.gz | sha256sum
   ```
2. Put them into `VER=` and `SUM=` in `build-mbelib-neo.sh` (the `Makefile` and the CI read the version from there), then `./build-mbelib-neo.sh`.
3. Check `mbelib_neo_transcode.c` against the new `mbelib-neo-$VER/src/src/internal`: the quantizers' signatures (`ambe_enc_*`, `mbe_ambe2450_quantize_*`, `mbe_imbe4400_quantize_amplitudes`), `struct ambe_enc_frame` and `struct ambe_enc_tables`, and what the file rebuilds from mbelib-neo's static functions (D-Star's pitch code, quantization and packing, the tone frames). Then put the version into `MBE_TRANSCODE_MBELIB`.
4. `make clean && make check`: the test checks all six directions of `transcode` against the audio path. The old `mbelib-neo-*` directory can go afterwards. In Docker or in `tcd/plugins/`: rebuild tcd, which rebuilds the plugin.

`MBELIB=` and `MBELIB_SRC=` name an installed library and its source tree elsewhere; both must be the pinned version.

## Use

In `/usr/local/etc/tcd.ini`, then restart tcd:

```ini
CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
```

- **No AMBE stick**: tcd runs D-Star and DMR in software, for any number of `Modules`.
- **One stick**: the stick does D-Star, the plugin DMR/YSF.
- **Two sticks**: the sticks do the first modules of `Modules`, the plugin the rest (DV3000 + DV3003 with `Modules = FSABC`: F and S in hardware, A, B and C in software).
- `CodecDStar = mbelib-neo` / `CodecDMR = mbelib-neo` put a codec in software from the start even where a stick could do it; tcd's web dashboard switches between the sticks and the plugin at run time.
- `CodecP25 = mbelib-neo` (tcd 0.14.0 and later) does P25 with the plugin instead of tcd's own OP25 vocoder; without it, or with an older tcd, P25 stays on OP25. The dashboard switches P25 at run time as well.
- tcd's monitor counts bit errors for D-Star and DMR only: tcd hands P25 over as the 88 IMBE parameter bits, without their FEC, so the plugin has no errors to count there (always 0), and a P25 frame is repeated, and muted after four repeats in a row, only when its pitch cannot be used.
- **Direct transcoding** (`Transcode = direct`, tcd 0.20.0's default): when a stream comes in as D-Star, DMR/YSF or P25 and goes out as another of the three, both through this plugin, the plugin codes the outgoing frame from the vocoder model (fundamental, voicing and amplitude of each harmonic) instead of from the decoded audio. The harmonics are laid on the target's pitch grid, the voicing of each band is decided by energy, and the amplitudes are coded with the target's quantizer against its own prediction; tcd's gain goes into the amplitudes. Tone frames (DTMF and single tones) become tone frames of the target where it has them, otherwise (P25) voiced harmonics on the tone frequencies; repeats play the last frame on, mutes become silence. `make check` tests all six directions against the audio path (level within 3 dB).

See "Codec plugins" in [tcd/README.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.md#codec-plugins) for the details.

## Measured

On a Raspberry Pi 4 (armhf) about 0.18 ms to encode and 0.04 ms to decode a 20 ms frame with the NEON routines (0.20 and 0.07 ms without, the frames encoded bit for bit the same), on a Pi 5 0.09 ms and 0.03 ms. Against a DVSI DV3003, frames go both ways: mbelib-neo frames decoded by the DV3003 and DV3003 frames decoded by mbelib-neo come out at the same level and envelope as the DV3003 on its own, for D-Star and DMR.

Sound against a DV3003 (2026-10-28, mbelib-neo 2.6.5, plugin 1.0.2; 1.0.3 to 1.1.1 code D-Star and DMR the same): PESQ (ITU-T P.862, narrowband, MOS from 1 to 4.5) against the original audio, mean over 19 clips (18 FM recordings, 1 speech clip, 8 kHz, at most the first 100 s of each), each pair of encoder and decoder:

| Encoder → decoder | DMR/YSF | D-Star |
|---|---|---|
| DV3003 → DV3003 | 2.52 | 2.48 |
| DV3003 → mbelib-neo | 2.50 | 2.40 |
| mbelib-neo → DV3003 | 2.49 | 2.59 |
| mbelib-neo → mbelib-neo | 2.46 | 2.50 |

With bit errors in the DV3003 frames (1 / 3 / 5 % of the bits flipped), decoded by the DV3003 and by mbelib-neo: DMR 2.47 / 2.37 / 2.15 against 2.45 / 2.32 / 2.14, D-Star 2.44 / 2.33 / 2.07 against 2.33 / 2.17 / 1.99.

- **DMR**: encoder and decoder as good as the DV3003, also with bit errors.
- **D-Star**: the encoder as good as the DV3003 or a little better (what radios hear from tcd), the decoder about 0.1 lower, also with bit errors.
- **P25** (no DVSI part does IMBE, so against tcd's own OP25 vocoder, on the same clips): OP25 → OP25 2.49, mbelib-neo → mbelib-neo 2.61, OP25 → mbelib-neo 2.53, mbelib-neo → OP25 2.55; through tcd to DMR 2.20 against 2.26, to M17 2.25 against 2.26. A small gain, mostly from the encoder; OP25 stays tcd's default.
- 19 clips and PESQ are a guide, not a listening test: differences under about 0.1 say little.

Transcoded directly (1.2.0, 2026-11-09, mbelib-neo 2.6.7): PESQ against the original over three speech clips (German female, English female and male, 8 kHz), each path with the plugin on both sides; the audio path decodes and encodes again, as tcd without `transcode` (both without tcd's AGC), measured with tcd's `codectest`:

| Source → target | audio path | direct | gain |
|---|---|---|---|
| D-Star → DMR | 2.56 | 2.80 | +0.24 |
| D-Star → P25 | 2.63 | 2.98 | +0.36 |
| DMR → D-Star | 2.54 | 2.81 | +0.27 |
| DMR → P25 | 2.60 | 2.90 | +0.30 |
| P25 → D-Star | 2.65 | 2.92 | +0.27 |
| P25 → DMR | 2.63 | 2.95 | +0.33 |

Each of the 18 cases gains (+0.16 to +0.44). The level is about 1 dB below the audio path's; `transcode` takes a fifth to a fifteenth of the time encoding takes. Three clips are fewer than the 19 above, but the direction is the same in every clip.

## Files

- `mbelib_neo_plugin.cpp`: the plugin
- `mbelib_neo_transcode.c`, `mbelib_neo_transcode.h`: `transcode`, against mbelib-neo 2.6.7's private headers
- `CodecPlugin.h`: a copy of tcd's plugin interface, kept the same as tcd's (`make check` compares it in `tcd/plugins/`) (`TCD_CODEC_API 4`, tcd 0.20.0 and later: `transcode`; tcd 0.15.0 to 0.19.x need plugin 1.1.2, tcd 0.12.0 to 0.14.x plugin 1.0.3); `make TCD=/path/to/urfd-ng/tcd` uses the one of a checkout instead
- `build-mbelib-neo.sh`: fetches and builds mbelib-neo
- `plugin.map`: the export list (Linux), `tests/smoke.cpp`: the test of `make check`

## Versions

- **1.2.0** (2026-11-09): tcd codec API 4 (tcd 0.20.0 and later): `transcode` for all six directions between D-Star, DMR/YSF and P25, from the vocoder model instead of the decoded audio; tones stay tones where the target has them; +0.24 to +0.36 PESQ over decoding and encoding again (see "Measured"). Uses mbelib-neo 2.6.7's private quantizers and builds with that version only. Encoding and decoding unchanged.
- **1.1.2** (2026-11-08): mbelib-neo 2.6.7. Its decoder comes closer to DVSI's: the voiced phase as DVSI regenerates it (the pitch moves 16 samples after mid-frame, a voiced onset restarts the phase), DMR's voicing (rate 33) as DVSI reads it, and DMR and P25 frames no longer scaled down where DVSI does not (up to 2.95 dB quieter on DMR, 1.05 dB on P25 before). The sound measurements below are of 2.6.5 still. `CodecPlugin.h` as in tcd 0.20.0 (API 4), the plugin stays API 3.
- **1.1.1** (2026-11-03): fixes from a review; encoding and decoding unchanged (bit for bit the same frames and PCM as 1.1.0). Only `tcd_codec_plugin` is exported, no exception leaves `open`, a frame is all zeros when encoding fails. `make check`, CI, `-Wall -Wextra`, `CXXFLAGS`/`LDFLAGS` from the environment, `WERROR=1`, `DESTDIR`. `build-mbelib-neo.sh` also on macOS, it checks its tools first and builds again when `libmbe-neo.a` is missing. The P25 statistics explained. `CodecPlugin.h` as in tcd 0.16.0.
- **1.1.0** (2026-10-31): tcd codec API 3 (tcd 0.15.0 and later): the decoder reports the corrected bit errors and the repeated, muted, tone and silence frames, which tcd's monitor counts; the frame formats are declared. Encoding and decoding unchanged.
- **1.0.3** (2026-10-28): P25 IMBE 4400, encoder and decoder from mbelib-neo (`CodecP25 = mbelib-neo`, tcd 0.14.0 and later); D-Star and DMR unchanged.
- **1.0.2** (2026-10-28): mbelib-neo 2.6.5 (the encoder no longer voices hum or background noise in pauses at the lowest pitches).
- **1.0.1** (2026-10-27): mbelib-neo 2.6.4 (decoder closer to DVSI's: lost frames, tones, D-Star bands); the plugin's version in tcd's log and monitor.
- **1.0.0** (2026-10-25): tcd codec API 2, mbelib-neo 2.6.2.

## License

GNU General Public License v3 or later, see [LICENSE](LICENSE). mbelib-neo is GPL-2.0-or-later with ISC and PFFFT parts, see its repository.

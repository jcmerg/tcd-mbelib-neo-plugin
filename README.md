# tcd-mbelib-neo-plugin

**English** | [Deutsch](README.de.md)

A codec plugin for **tcd**, the transcoder of [urfd-ng](https://github.com/jcmerg/urfd-ng), built on [mbelib-neo](https://github.com/arancormonk/mbelib-neo). It encodes and decodes in software:

- D-Star (AMBE 3600x2400)
- DMR, YSF and NXDN (AMBE+2 3600x2450)
- P25 and YSF VW (IMBE 4400), as a choice next to tcd's own OP25 vocoder

With it, tcd transcodes without DVSI hardware, or more modules than the AMBE sticks have channels. Between D-Star, DMR/YSF and P25 it also transcodes directly (`Transcode = direct`): the outgoing frame is made from the vocoder parameters of the incoming one, without decoding to audio and encoding again, which sounds noticeably better.

**Requirement**: tcd 0.23.0 or later (urfd-ng's `tcd/`). tcd 0.20.0 to 0.22.x need plugin 1.2.4 (there the `op25` and `codec2` lines under "Use" need tcd 0.22.0), 0.15.0 to 0.19.x plugin 1.1.2, 0.12.0 to 0.14.x plugin 1.0.3. The release notes are on the [releases page](https://github.com/jcmerg/tcd-mbelib-neo-plugin/releases).

## Patent notice

**Read this before you build or use it.** Software implementations of AMBE and AMBE+2 may implicate patents. mbelib-neo's own [patent research record](https://github.com/arancormonk/mbelib-neo/blob/main/legal/patents.md) lists US 8,359,197 (AMBE+2 half rate) as active, with a projected expiry of 2028-05-20, and further open records. In Europe its counterparts (EP 1 465 158, EP 1 748 425) and DVSI's older patents ran out after 20 years, by 2024-03-26 at the latest; a search of DVSI's European patents and applications (2026-10-28) found no granted patent in force whose method this plugin uses, but applications are pending and can still be granted. No patent license is given or implied by this plugin, by mbelib-neo or by the GPL. Whether to build and use it, and where, is your decision; this is no legal advice. This plugin is therefore not part of urfd-ng: its setup and builds take it only when you clone it into `tcd/plugins/` yourself, and it is off unless you load it.

On direct transcoding: DVSI patented converting between MBE vocoders at the parameter level, as "Voice transcoder" (US 7,634,399, with its divisional US 7,957,963) and in the "Interoperable vocoder" family (US 7,970,606, US 8,315,860, EP 1 420 390 with DE 603 16 396, CA 2,447,735, JP 4,166,673). According to Google Patents (retrieved 2026-11-09) all of them have expired, US 7,634,399 on 2025-11-07 and EP 1 420 390 on 2023-11-07. Newer DVSI applications the search found are, by their titles, about other things (tone detection, error correction). `transcode` uses the AMBE+2 quantizer, for which US 8,359,197 above still applies. This too is research, not a complete search and not legal advice; Google Patents' status is not an official register.

## Build

Needs a C/C++ compiler, `make`, `cmake` (3.13 or later), `tar`, `curl` or `wget`, and `sha256sum` or `shasum` (Debian, Ubuntu, Raspberry Pi OS; x86_64, arm64, armhf; macOS builds it too).

```bash
git clone https://github.com/jcmerg/tcd-mbelib-neo-plugin.git
cd tcd-mbelib-neo-plugin
./build-mbelib-neo.sh        # downloads mbelib-neo (checked against its SHA-256) and builds it here
make
make check                   # loads the plugin as tcd does and runs each codec once
sudo make install            # /usr/local/lib/tcd/mbelib_neo_codec.so
```

Or clone this repository into `tcd/plugins/mbelib-neo` of the urfd-ng checkout: the tcd Docker image and `tcd/build.sh` then build and install it with tcd, and `setup.sh update` pulls it (see "Codec plugins" and "tcd in Docker" in [tcd/README.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.md#codec-plugins)).

Build options: `CFLAGS`, `CXXFLAGS`, `CPPFLAGS` and `LDFLAGS` from the environment are taken (default `-O2`), `make WERROR=1` makes warnings errors, `make DESTDIR=… install` installs into a staging directory. mbelib-neo is linked statically and only `tcd_codec_plugin` is exported. On 32-bit ARM (armhf) the script switches mbelib-neo's NEON routines on when the CPU or the cross compiler's target has NEON; `MBELIB_NEON=1` or `0` decides it yourself. The binaries stay portable.

To test it by hand, urfd-ng has `tcd/tools/` (`make` there): `./codectest /usr/local/lib/tcd/mbelib_neo_codec.so dmr|dstar|p25 in.raw out.frames out.raw` encodes and decodes 8 kHz s16le PCM and prints the time per frame; with two codecs (e.g. `dstar:dmr`) it transcodes directly; `dvsitest` compares with a DV3003.

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

In `/usr/local/etc/tcd.ini` (tcd in Docker: `$URFD_DATA/tcd/tcd.ini`), then restart tcd:

```ini
CodecPlugin = op25           # tcd's own vocoders: keep these two lines
CodecPlugin = codec2
CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
```

- **No AMBE stick**: tcd runs D-Star and DMR in software, for any number of `Modules`.
- **One stick**: the stick does D-Star, the plugin DMR/YSF.
- **Two sticks**: the sticks do the first modules of `Modules`, the plugin the rest (DV3000 + DV3003 with `Modules = FSABC`: F and S in hardware, A, B and C in software).
- `CodecDStar = mbelib-neo` / `CodecDMR = mbelib-neo` put a codec in software from the start even where a stick could do it; `CodecP25 = mbelib-neo` does P25 with the plugin instead of OP25. tcd's web dashboard switches between sticks, plugin and OP25 at run time (keep `CodecPlugin = op25`, or OP25 is no longer offered there).
- **Direct transcoding** (`Transcode = direct`, tcd's default): when a stream comes in as D-Star, DMR/YSF or P25 and goes out as another of the three, both through this plugin, the outgoing frame is coded from the vocoder model (fundamental, voicing and amplitudes of the harmonics), laid on the target's pitch grid and quantized with the target's quantizer; tcd's gain goes into the amplitudes. Tones (DTMF and single tones) stay tones where the target has them, repeats play the last frame on, mutes become silence.
- tcd's monitor counts bit errors for D-Star and DMR only: tcd hands P25 over as the 88 IMBE parameter bits without their FEC, so there are no errors to count (always 0).

See "Codec plugins" in [tcd/README.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.md#codec-plugins) for the details.

## Quality

Measured with PESQ against the original audio on FM recordings and clean speech; the figures and how they were measured are in [docs/MEASUREMENTS.md](docs/MEASUREMENTS.md).

- **DMR**: encoder and decoder as good as a DVSI DV3003, also with bit errors.
- **D-Star**: the encoder as good as the DV3003 or a little better, the decoder close to it.
- **P25**: a little better than tcd's own OP25 vocoder, mostly from the encoder; OP25 stays tcd's default.
- **Direct transcoding**: +0.37 to +0.46 PESQ over decoding and encoding again.
- **Speed**: on a Raspberry Pi 4 about 0.18 ms to encode and 0.04 ms to decode a 20 ms frame, on a Pi 5 0.09 and 0.03 ms.

## Files

- `mbelib_neo_plugin.cpp`: the plugin
- `mbelib_neo_transcode.c`, `mbelib_neo_transcode.h`: direct transcoding, against mbelib-neo's private headers
- `CodecPlugin.h`: a copy of tcd's plugin interface (`make check` compares it in `tcd/plugins/`); `make TCD=/path/to/urfd-ng/tcd` uses the one of a checkout instead
- `build-mbelib-neo.sh`: fetches and builds mbelib-neo
- `plugin.map`: the export list (Linux), `tests/`: the tests of `make check`
- `docs/MEASUREMENTS.md`: speed and sound, measured

## License

GNU General Public License v3 or later, see [LICENSE](LICENSE). mbelib-neo is GPL-2.0-or-later with ISC and PFFFT parts, see its repository.

# tcd-mbelib-neo-plugin

[English](README.md) | [Deutsch](README.de.md)

A codec plugin for **tcd**, the transcoder of [urfd-ng](https://github.com/jcmerg/urfd-ng): D-Star (AMBE 3600x2400) and DMR/YSF/NXDN (AMBE+2 3600x2450) in software, encoder and decoder, from [mbelib-neo](https://github.com/arancormonk/mbelib-neo). With it, tcd transcodes without DVSI hardware, or more modules than the AMBE sticks have channels.

## Patent notice

**Read this before you build or use it.** Software implementations of AMBE and AMBE+2 may implicate patents. mbelib-neo's own [patent research record](https://github.com/arancormonk/mbelib-neo/blob/main/legal/patents.md) lists US 8,359,197 (AMBE+2 half rate) as active, with a projected expiry of 2028-05-20, and further open records. No patent license is given or implied by this plugin, by mbelib-neo or by the GPL. Whether to build and use it, and where, is your decision; this is no legal advice. This plugin is therefore not part of urfd-ng: its setup and builds take it only when you clone it into `tcd/plugins/` yourself, and it is off unless you load it.

## Build

Needs a C/C++ compiler, `make`, `cmake` and `curl` or `wget` (Debian, Ubuntu, Raspberry Pi OS; x86_64, arm64, armhf).

```bash
git clone https://github.com/jcmerg/tcd-mbelib-neo-plugin.git
cd tcd-mbelib-neo-plugin
./build-mbelib-neo.sh        # downloads mbelib-neo 2.6.4 (checked against its SHA-256) and builds it here
make
sudo make install            # /usr/local/lib/tcd/mbelib_neo_codec.so
```

Or clone this repository into `tcd/plugins/mbelib-neo` of the urfd-ng checkout: the tcd Docker image and `tcd/build.sh` build it with tcd and install it, and `setup.sh update` pulls it (see "Codec plugins" and "tcd in Docker" in [tcd/README.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.md#codec-plugins)).

To test it, urfd-ng has `tcd/tools/` (`make` there): `./codectest /usr/local/lib/tcd/mbelib_neo_codec.so dmr in.raw out.frames out.raw` encodes and decodes 8 kHz s16le PCM and prints the time per frame; `dvsitest` compares with a DV3003.

mbelib-neo is not part of this repository: `build-mbelib-neo.sh` fetches its release source archive and the plugin links it statically. Only the D-Star and AMBE+2 parts end up in the plugin. On 64-bit hosts mbelib-neo uses its SIMD routines by default; on 32-bit ARM (armhf, e.g. Raspberry Pi OS) the script switches its NEON routines on when the CPU has NEON (Raspberry Pi 2 and later). The binaries stay portable (`MBELIB_ENABLE_NATIVE=OFF`).

### Updating mbelib-neo

The script builds one version, checked against its SHA-256, so a build is the same everywhere. For a newer release of [mbelib-neo](https://github.com/arancormonk/mbelib-neo/releases):

```bash
VER=2.7.0   # the new version
curl -fsSL https://github.com/arancormonk/mbelib-neo/releases/download/v$VER/mbelib-neo-$VER-source.tar.gz | sha256sum
```

Put the version and the sum into `VER=` and `SUM=` in `build-mbelib-neo.sh` and the version into the `Makefile`, remove the old `mbelib-neo-*` directory, then `./build-mbelib-neo.sh && make clean && make` (in Docker or in `tcd/plugins/`: rebuild tcd, which rebuilds the plugin). Read its release notes first: a new major version can change the API.

## Use

In `/usr/local/etc/tcd.ini`, then restart tcd:

```ini
CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
```

- **No AMBE stick**: tcd runs D-Star and DMR in software, for any number of `Modules`.
- **One stick**: the stick does D-Star, the plugin DMR/YSF.
- **Two sticks**: the sticks do the first modules of `Modules`, the plugin the rest (DV3000 + DV3003 with `Modules = FSABC`: F and S in hardware, A, B and C in software).
- `CodecDStar = mbelib-neo` / `CodecDMR = mbelib-neo` put a codec in software from the start even where a stick could do it; tcd's web dashboard switches between the sticks and the plugin at run time.

See "Codec plugins" in [tcd/README.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.md#codec-plugins) for the details.

## Measured

On a Raspberry Pi 4 (armhf) about 0.18 ms to encode and 0.04 ms to decode a 20 ms frame with the NEON routines (0.20 and 0.07 ms without, the frames encoded bit for bit the same), on a Pi 5 0.09 ms and 0.03 ms. Against a DVSI DV3003, frames go both ways: mbelib-neo frames decoded by the DV3003 and DV3003 frames decoded by mbelib-neo come out at the same level and envelope as the DV3003 on its own, for D-Star and DMR.

Sound against a DV3003 (2026-10-27): PESQ (ITU-T P.862, narrowband, MOS from 1 to 4.5) against the original audio, mean over 5 clips (4 FM recordings, 1 speech clip, 8 kHz), each pair of encoder and decoder, with mbelib-neo 2.6.4 (plugin 1.0.1) and 2.6.2 (plugin 1.0.0) before it:

| Encoder → decoder | DMR/YSF, 2.6.4 | D-Star, 2.6.4 | DMR/YSF, 2.6.2 | D-Star, 2.6.2 |
|---|---|---|---|---|
| DV3003 → DV3003 | 2.73 | 2.79 | 2.73 | 2.79 |
| DV3003 → mbelib-neo | 2.73 | 2.64 | 2.69 | 2.60 |
| mbelib-neo → DV3003 | 2.76 | 2.85 | 2.76 | 2.47 |
| mbelib-neo → mbelib-neo | 2.69 | 2.70 | 2.68 | 2.69 |

With bit errors in the DV3003 frames (1 / 3 / 5 % of the bits flipped), decoded by the DV3003 and by mbelib-neo 2.6.4: DMR 2.69 / 2.55 / 2.31 against 2.65 / 2.53 / 2.33, D-Star 2.73 / 2.56 / 2.21 against 2.57 / 2.39 / 2.14.

- **DMR**: encoder and decoder as good as the DV3003, also with bit errors.
- **D-Star**: the encoder as good as the DV3003 (what radios hear from tcd; 2.6.2 made frames that the DV3003 plays as lost or silent), the decoder about 0.15 lower, also with bit errors.
- Five clips and PESQ are a guide, not a listening test: differences under about 0.1 say little.

## Files

- `mbelib_neo_plugin.cpp`: the plugin
- `CodecPlugin.h`: a copy of tcd's plugin interface (`TCD_CODEC_API 2`, tcd 0.12.0 and later: every transmission begins with a reset encoder and decoder; an older tcd needs the plugin before this change); `make TCD=/path/to/urfd-ng/tcd` uses the one of a checkout instead
- `build-mbelib-neo.sh`: fetches and builds mbelib-neo

## Versions

- **1.0.1** (2026-10-27): mbelib-neo 2.6.4 (decoder closer to DVSI's: lost frames, tones, D-Star bands); the plugin's version in tcd's log and monitor.
- **1.0.0** (2026-10-25): tcd codec API 2, mbelib-neo 2.6.2.

## License

GNU General Public License v3 or later, see [LICENSE](LICENSE). mbelib-neo is GPL-2.0-or-later with ISC and PFFFT parts, see its repository.

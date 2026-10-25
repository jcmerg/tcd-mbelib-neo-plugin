# tcd-mbelib-neo-plugin

[English](README.md) | [Deutsch](README.de.md)

A codec plugin for **tcd**, the transcoder of [urfd-ng](https://github.com/jcmerg/urfd-ng): D-Star (AMBE 3600x2400) and DMR/YSF/NXDN (AMBE+2 3600x2450) in software, encoder and decoder, from [mbelib-neo](https://github.com/arancormonk/mbelib-neo). With it, tcd transcodes without DVSI hardware, or more modules than the AMBE sticks have channels.

## Patent notice

**Read this before you build or use it.** Software implementations of AMBE and AMBE+2 may implicate patents. mbelib-neo's own [patent research record](https://github.com/arancormonk/mbelib-neo/blob/main/legal/patents.md) lists US 8,359,197 (AMBE+2 half rate) as active, with a projected expiry of 2028-05-20, and further open records. No patent license is given or implied by this plugin, by mbelib-neo or by the GPL. Whether to build and use it, and where, is your decision; this is no legal advice. This plugin is therefore not part of urfd-ng, not built by its setup or Docker image, and off unless you load it.

## Build

Needs a C/C++ compiler, `make`, `cmake` and `curl` or `wget` (Debian, Ubuntu, Raspberry Pi OS; x86_64, arm64, armhf).

```bash
git clone https://github.com/jcmerg/tcd-mbelib-neo-plugin.git
cd tcd-mbelib-neo-plugin
./build-mbelib-neo.sh        # downloads mbelib-neo 2.6.2 (checked against its SHA-256) and builds it here
make
sudo make install            # /usr/local/lib/tcd/mbelib_neo_codec.so
```

For tcd in Docker, clone this repository into `tcd/plugins/mbelib-neo` of the urfd-ng checkout instead; the image builds it (see "tcd in Docker" in [tcd/README.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.md#tcd-in-docker)).

mbelib-neo is not part of this repository: `build-mbelib-neo.sh` fetches its release source archive and the plugin links it statically. Only the D-Star and AMBE+2 parts end up in the plugin.

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

On a Raspberry Pi 4 (armhf) about 0.20 ms to encode and 0.08 ms to decode a 20 ms frame, on a Pi 5 0.09 ms and 0.03 ms. Against a DVSI DV3003, frames go both ways: mbelib-neo frames decoded by the DV3003 and DV3003 frames decoded by mbelib-neo come out at the same level and envelope as the DV3003 on its own, for D-Star and DMR.

## Files

- `mbelib_neo_plugin.cpp`: the plugin
- `CodecPlugin.h`: a copy of tcd's plugin interface (`TCD_CODEC_API 1`); `make TCD=/path/to/urfd-ng/tcd` uses the one of a checkout instead
- `build-mbelib-neo.sh`: fetches and builds mbelib-neo

## License

GNU General Public License v3 or later, see [LICENSE](LICENSE). mbelib-neo is GPL-2.0-or-later with ISC and PFFFT parts, see its repository.

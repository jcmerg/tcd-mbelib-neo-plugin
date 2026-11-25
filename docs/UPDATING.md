# Updating mbelib-neo

[English](UPDATING.md) | [Deutsch](UPDATING.de.md)

The plugin is pinned to exactly one mbelib-neo version, so a build is the same everywhere: `build-mbelib-neo.sh` fetches it and checks its SHA-256. `transcode` (`mbelib_neo_transcode.c`) uses mbelib-neo's private headers and quantizers for it (`src/internal`, in the static archive), which mbelib-neo does not promise to keep stable; so the plugin builds only with the version that file is written for (`MBE_TRANSCODE_MBELIB` in `mbelib_neo_transcode.h`). The `Makefile` checks it on the source tree, `mbelib_neo_plugin.cpp` on the installed library; another version stops the build rather than sending wrong frames.

For a newer release of [mbelib-neo](https://github.com/arancormonk/mbelib-neo/releases), after reading its release notes:

1. Get the sum:
   ```bash
   VER=2.7.0   # the new version
   curl -fsSL https://github.com/arancormonk/mbelib-neo/releases/download/v$VER/mbelib-neo-$VER-source.tar.gz | sha256sum
   ```
2. Put them into `VER=` and `SUM=` in `build-mbelib-neo.sh` (the `Makefile` and the CI read the version from there), then `./build-mbelib-neo.sh`.
3. Check `mbelib_neo_transcode.c` against the new `mbelib-neo-$VER/src/src/internal`: the quantizers' signatures (`ambe_enc_*`, `mbe_ambe2450_quantize_*`, `mbe_imbe4400_quantize_amplitudes`), `struct ambe_enc_frame` and `struct ambe_enc_tables`, and what the file rebuilds from mbelib-neo's static functions (D-Star's pitch code, quantization and packing, the tone frames, and the fixed pitch codes of frames with no voiced band: `AMBE2400_UNVOICED_B0`, `AMBE2450_UNVOICED_VUV`/`_B0`, `IMBE_UNVOICED_B0` from the encoders' `*_unvoiced_grid`). Then put the version into `MBE_TRANSCODE_MBELIB`.
4. `make clean && make check`: the tests check all six directions of `transcode` against the audio path, and the pitch code of its frames with no voiced band. The old `mbelib-neo-*` directory can go afterwards. In Docker or in `tcd/plugins/`: rebuild tcd, which rebuilds the plugin.

`MBELIB=` and `MBELIB_SRC=` name an installed library and its source tree elsewhere; both must be the pinned version.

# mbelib-neo aktualisieren

[English](UPDATING.md) | [Deutsch](UPDATING.de.md)

Das Plugin ist auf genau eine Version von mbelib-neo festgelegt, damit ein Build überall gleich ist: `build-mbelib-neo.sh` holt sie und prüft ihre SHA-256. `transcode` (`mbelib_neo_transcode.c`) nutzt dazu die privaten Header und Quantisierer von mbelib-neo (`src/internal`, im statischen Archiv), für die mbelib-neo keine Stabilität verspricht; deshalb baut das Plugin nur mit der Version, für die diese Datei geschrieben ist (`MBE_TRANSCODE_MBELIB` in `mbelib_neo_transcode.h`). Das `Makefile` prüft sie am Quellbaum, `mbelib_neo_plugin.cpp` an der installierten Bibliothek; eine andere Version bricht den Build ab, statt falsche Frames zu senden.

Für eine neuere Version von [mbelib-neo](https://github.com/arancormonk/mbelib-neo/releases), vorher ihre Release-Notes lesen:

1. Summe holen:
   ```bash
   VER=2.7.0   # die neue Version
   curl -fsSL https://github.com/arancormonk/mbelib-neo/releases/download/v$VER/mbelib-neo-$VER-source.tar.gz | sha256sum
   ```
2. `VER=` und `SUM=` in `build-mbelib-neo.sh` eintragen (das `Makefile` und die CI lesen die Version von dort), dann `./build-mbelib-neo.sh`.
3. `mbelib_neo_transcode.c` gegen das neue `mbelib-neo-$VER/src/src/internal` prüfen: die Signaturen der Quantisierer (`ambe_enc_*`, `mbe_ambe2450_quantize_*`, `mbe_imbe4400_quantize_amplitudes`), `struct ambe_enc_frame` und `struct ambe_enc_tables`, und was die Datei aus statischen Funktionen von mbelib-neo nachbildet (Pitch-Code, Quantisierung und Packen von D-Star, die Tonframes, und die festen Pitch-Codes von Frames ohne stimmhaftes Band: `AMBE2400_UNVOICED_B0`, `AMBE2450_UNVOICED_VUV`/`_B0`, `IMBE_UNVOICED_B0` aus den `*_unvoiced_grid` der Encoder). Dann die Version in `MBE_TRANSCODE_MBELIB` eintragen.
4. `make clean && make check`: Die Tests prüfen alle sechs Richtungen von `transcode` gegen den Audio-Weg und den Pitch-Code seiner Frames ohne stimmhaftes Band. Das alte Verzeichnis `mbelib-neo-*` kann danach weg. In Docker oder in `tcd/plugins/`: tcd neu bauen, das baut das Plugin mit.

`MBELIB=` und `MBELIB_SRC=` nennen eine installierte Bibliothek und ihren Quellbaum an anderer Stelle; beide müssen die festgelegte Version haben.

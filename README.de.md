# tcd-mbelib-neo-plugin

[English](README.md) | **Deutsch**

Ein Codec-Plugin für **tcd**, den Transcoder von [urfd-ng](https://github.com/jcmerg/urfd-ng), auf Basis von [mbelib-neo](https://github.com/arancormonk/mbelib-neo). Es kodiert und dekodiert in Software:

- D-Star (AMBE 3600x2400)
- DMR, YSF und NXDN (AMBE+2 3600x2450)
- P25 und YSF VW (IMBE 4400), als Wahl neben dem eigenen OP25-Vocoder von tcd

Damit transcodiert tcd ohne DVSI-Hardware oder mehr Module, als die AMBE-Sticks Kanäle haben. Zwischen D-Star, DMR/YSF und P25 transcodiert es auch direkt (`Transcode = direct`): Der ausgehende Frame entsteht aus den Vocoder-Parametern des eingehenden, ohne Dekodieren zu Audio und erneutes Kodieren, was hörbar besser klingt.

**Voraussetzung**: tcd 0.20.0 oder neuer (`tcd/` von urfd-ng); die Zeilen `op25` und `codec2` unter „Benutzen“ brauchen tcd 0.22.0, mit einem älteren weglassen. tcd 0.15.0 bis 0.19.x brauchen Plugin 1.1.2, 0.12.0 bis 0.14.x Plugin 1.0.3. Die Release Notes stehen auf der [Releases-Seite](https://github.com/jcmerg/tcd-mbelib-neo-plugin/releases).

## Patenthinweis

**Bitte vor dem Bauen und Benutzen lesen.** Software-Umsetzungen von AMBE und AMBE+2 können Patente berühren. Die [Patentübersicht](https://github.com/arancormonk/mbelib-neo/blob/main/legal/patents.md) von mbelib-neo führt US 8,359,197 (AMBE+2 Halbrate) als aktiv, voraussichtlich bis 20.05.2028, und weitere offene Einträge. In Europa sind seine Gegenstücke (EP 1 465 158, EP 1 748 425) und die älteren DVSI-Patente nach 20 Jahren abgelaufen, spätestens am 26.03.2024; eine Suche in DVSIs europäischen Patenten und Anmeldungen (28.10.2026) fand kein geltendes erteiltes Patent, dessen Verfahren dieses Plugin nutzt, es sind aber Anmeldungen offen, die noch erteilt werden können. Weder dieses Plugin noch mbelib-neo noch die GPL geben eine Patentlizenz. Ob und wo du es baust und nutzt, entscheidest du; das hier ist keine Rechtsberatung. Deshalb gehört das Plugin nicht zu urfd-ng: dessen Setup und Builds nehmen es nur, wenn du es selbst nach `tcd/plugins/` klonst, und es ist aus, solange du es nicht lädst.

Zum direkten Transcodieren: DVSI hatte das Umsetzen zwischen MBE-Vocodern auf Ebene der Parameter patentiert, als „Voice transcoder“ (US 7,634,399, dazu die Teilanmeldung US 7,957,963) und in der Familie „Interoperable vocoder“ (US 7,970,606, US 8,315,860, EP 1 420 390 mit DE 603 16 396, CA 2,447,735, JP 4,166,673). Laut Google Patents (abgerufen am 09.11.2026) sind alle abgelaufen, US 7,634,399 am 07.11.2025 und EP 1 420 390 am 07.11.2023. Neuere Anmeldungen von DVSI, die die Suche fand, betreffen nach ihrem Titel anderes (Tonerkennung, Fehlerkorrektur). `transcode` nutzt den Quantisierer von AMBE+2, für den US 8,359,197 oben weiter gilt. Auch das ist eine Recherche, keine vollständige Suche und keine Rechtsauskunft; der Status bei Google Patents ist keine amtliche Auskunft.

## Bauen

Braucht einen C/C++-Compiler, `make`, `cmake` (3.13 oder neuer), `tar`, `curl` oder `wget` sowie `sha256sum` oder `shasum` (Debian, Ubuntu, Raspberry Pi OS; x86_64, arm64, armhf; auch macOS baut es).

```bash
git clone https://github.com/jcmerg/tcd-mbelib-neo-plugin.git
cd tcd-mbelib-neo-plugin
./build-mbelib-neo.sh        # lädt mbelib-neo (gegen seine SHA-256 geprüft) und baut es hier
make
make check                   # lädt das Plugin wie tcd und lässt jeden Codec einmal laufen
sudo make install            # /usr/local/lib/tcd/mbelib_neo_codec.so
```

Oder dieses Repository nach `tcd/plugins/mbelib-neo` des urfd-ng-Klons klonen: Das tcd-Docker-Image und `tcd/build.sh` bauen und installieren es dann mit tcd, `setup.sh update` holt es auf den neuen Stand (siehe „Codec-Plugins“ und „tcd in Docker“ in [tcd/README.de.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.de.md#codec-plugins)).

Build-Optionen: `CFLAGS`, `CXXFLAGS`, `CPPFLAGS` und `LDFLAGS` aus der Umgebung werden übernommen (Standard `-O2`), `make WERROR=1` macht Warnungen zu Fehlern, `make DESTDIR=… install` installiert in ein Staging-Verzeichnis. mbelib-neo wird statisch gelinkt, exportiert wird nur `tcd_codec_plugin`. Auf 32-Bit-ARM (armhf) schaltet das Skript die NEON-Routinen von mbelib-neo ein, wenn die CPU oder das Ziel des Cross-Compilers NEON kann; `MBELIB_NEON=1` oder `0` legt es selbst fest. Die Binaries bleiben übertragbar.

Zum Testen von Hand hat urfd-ng `tcd/tools/` (`make` dort): `./codectest /usr/local/lib/tcd/mbelib_neo_codec.so dmr|dstar|p25 ein.raw aus.frames aus.raw` kodiert und dekodiert 8-kHz-s16le-PCM und gibt die Zeit pro Frame aus; mit zwei Codecs (z. B. `dstar:dmr`) transcodiert es direkt; `dvsitest` vergleicht mit einem DV3003.

### mbelib-neo aktualisieren

Das Plugin ist auf genau eine Version von mbelib-neo festgelegt, damit ein Build überall gleich ist: `build-mbelib-neo.sh` holt sie und prüft ihre SHA-256. `transcode` (`mbelib_neo_transcode.c`) nutzt dazu die privaten Header und Quantisierer von mbelib-neo (`src/internal`, im statischen Archiv), für die mbelib-neo keine Stabilität verspricht; deshalb baut das Plugin nur mit der Version, für die diese Datei geschrieben ist (`MBE_TRANSCODE_MBELIB` in `mbelib_neo_transcode.h`). Das `Makefile` prüft sie am Quellbaum, `mbelib_neo_plugin.cpp` an der installierten Bibliothek; eine andere Version bricht den Build ab, statt falsche Frames zu senden.

Für eine neuere Version von [mbelib-neo](https://github.com/arancormonk/mbelib-neo/releases), vorher ihre Release-Notes lesen:

1. Summe holen:
   ```bash
   VER=2.7.0   # die neue Version
   curl -fsSL https://github.com/arancormonk/mbelib-neo/releases/download/v$VER/mbelib-neo-$VER-source.tar.gz | sha256sum
   ```
2. `VER=` und `SUM=` in `build-mbelib-neo.sh` eintragen (das `Makefile` und die CI lesen die Version von dort), dann `./build-mbelib-neo.sh`.
3. `mbelib_neo_transcode.c` gegen das neue `mbelib-neo-$VER/src/src/internal` prüfen: die Signaturen der Quantisierer (`ambe_enc_*`, `mbe_ambe2450_quantize_*`, `mbe_imbe4400_quantize_amplitudes`), `struct ambe_enc_frame` und `struct ambe_enc_tables`, und was die Datei aus statischen Funktionen von mbelib-neo nachbildet (Pitch-Code, Quantisierung und Packen von D-Star, die Tonframes). Dann die Version in `MBE_TRANSCODE_MBELIB` eintragen.
4. `make clean && make check`: Der Test prüft alle sechs Richtungen von `transcode` gegen den Audio-Weg. Das alte Verzeichnis `mbelib-neo-*` kann danach weg. In Docker oder in `tcd/plugins/`: tcd neu bauen, das baut das Plugin mit.

`MBELIB=` und `MBELIB_SRC=` nennen eine installierte Bibliothek und ihren Quellbaum an anderer Stelle; beide müssen die festgelegte Version haben.

## Benutzen

In `/usr/local/etc/tcd.ini` (tcd in Docker: `$URFD_DATA/tcd/tcd.ini`), dann tcd neu starten:

```ini
CodecPlugin = op25           # die eigenen Vocoder von tcd: diese zwei Zeilen behalten
CodecPlugin = codec2
CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
```

- **Kein AMBE-Stick**: tcd macht D-Star und DMR in Software, für beliebig viele `Modules`.
- **Ein Stick**: Der Stick macht D-Star, das Plugin DMR/YSF.
- **Zwei Sticks**: Die Sticks übernehmen die ersten Module aus `Modules`, das Plugin den Rest (DV3000 + DV3003 mit `Modules = FSABC`: F und S in Hardware, A, B und C in Software).
- `CodecDStar = mbelib-neo` / `CodecDMR = mbelib-neo` legen einen Codec von Anfang an in Software, auch wo ein Stick ihn könnte; `CodecP25 = mbelib-neo` macht P25 mit dem Plugin statt mit OP25. Das Web-Dashboard von tcd schaltet zur Laufzeit zwischen Sticks, Plugin und OP25 um (`CodecPlugin = op25` behalten, sonst bietet es OP25 nicht mehr an).
- **Direkt transcodieren** (`Transcode = direct`, der Standard von tcd): Kommt ein Stream als D-Star, DMR/YSF oder P25 herein und geht als ein anderer der drei hinaus, beide über dieses Plugin, wird der ausgehende Frame aus dem Vocoder-Modell kodiert (Grundfrequenz, Stimmhaftigkeit und Amplituden der Harmonischen), auf das Tonhöhenraster des Ziels gelegt und mit dessen Quantisierer kodiert; der Gain von tcd geht in die Amplituden ein. Töne (DTMF und Einzeltöne) bleiben Töne, wo das Ziel sie hat, Wiederholungen spielen den letzten Frame weiter, Stummschaltungen werden Stille.
- Der Monitor von tcd zählt Bitfehler nur für D-Star und DMR: tcd gibt P25 als die 88 IMBE-Parameterbits ohne ihre FEC weiter, es gibt also keine Fehler zu zählen (immer 0).

Einzelheiten unter „Codec-Plugins“ in [tcd/README.de.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.de.md#codec-plugins).

## Qualität

Gemessen mit PESQ gegen das Original-Audio auf FM-Mitschnitten und sauberer Sprache; die Zahlen und wie sie gemessen wurden, stehen in [docs/MEASUREMENTS.de.md](docs/MEASUREMENTS.de.md).

- **DMR**: Encoder und Decoder so gut wie ein DVSI DV3003, auch mit Bitfehlern.
- **D-Star**: Der Encoder so gut wie der DV3003 oder etwas besser, der Decoder nah dran.
- **P25**: etwas besser als der eigene OP25-Vocoder von tcd, vor allem durch den Encoder; OP25 bleibt der Standard von tcd.
- **Direkt transcodieren**: +0,37 bis +0,46 PESQ gegenüber Dekodieren und erneutem Kodieren.
- **Geschwindigkeit**: auf einem Raspberry Pi 4 etwa 0,18 ms zum Kodieren und 0,04 ms zum Dekodieren eines 20-ms-Frames, auf einem Pi 5 0,09 und 0,03 ms.

## Dateien

- `mbelib_neo_plugin.cpp`: das Plugin
- `mbelib_neo_transcode.c`, `mbelib_neo_transcode.h`: direktes Transcodieren, gegen die privaten Header von mbelib-neo
- `CodecPlugin.h`: eine Kopie der Plugin-Schnittstelle von tcd (`make check` vergleicht sie in `tcd/plugins/`); `make TCD=/pfad/zu/urfd-ng/tcd` nimmt stattdessen die eines Checkouts
- `build-mbelib-neo.sh`: holt und baut mbelib-neo
- `plugin.map`: die Exportliste (Linux), `tests/`: die Tests von `make check`
- `docs/MEASUREMENTS.de.md`: Geschwindigkeit und Klang, gemessen

## Lizenz

GNU General Public License v3 oder neuer, siehe [LICENSE](LICENSE). mbelib-neo steht unter GPL-2.0-or-later mit Teilen unter ISC und PFFFT, siehe sein Repository.

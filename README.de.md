# tcd-mbelib-neo-plugin

[English](README.md) | [Deutsch](README.de.md)

Ein Codec-Plugin für **tcd**, den Transcoder von [urfd-ng](https://github.com/jcmerg/urfd-ng): D-Star (AMBE 3600x2400) und DMR/YSF/NXDN (AMBE+2 3600x2450) in Software, Encoder und Decoder, aus [mbelib-neo](https://github.com/arancormonk/mbelib-neo). Damit transcodiert tcd ohne DVSI-Hardware oder mehr Module, als die AMBE-Sticks Kanäle haben. Dazu kann es P25 (IMBE 4400), als Wahl neben dem eigenen OP25-Vocoder von tcd; urfd-ng 4.31.0 und neuer geben auch YSF VW (Voice Wide, dasselbe IMBE) als P25 an tcd.

Seit 1.2.0 transcodiert es D-Star, DMR/YSF und P25 auch direkt ineinander (`transcode`, tcd 0.20.0 und neuer, `Transcode = direct`): Der ausgehende Frame entsteht aus den Parametern des Vocoders, die der Decoder aus dem eingehenden gelesen hat, ohne Synthese und zweite Analyse.

## Patenthinweis

**Bitte vor dem Bauen und Benutzen lesen.** Software-Umsetzungen von AMBE und AMBE+2 können Patente berühren. Die [Patentübersicht](https://github.com/arancormonk/mbelib-neo/blob/main/legal/patents.md) von mbelib-neo führt US 8,359,197 (AMBE+2 Halbrate) als aktiv, voraussichtlich bis 20.05.2028, und weitere offene Einträge. In Europa sind seine Gegenstücke (EP 1 465 158, EP 1 748 425) und die älteren DVSI-Patente nach 20 Jahren abgelaufen, spätestens am 26.03.2024; eine Suche in DVSIs europäischen Patenten und Anmeldungen (28.10.2026) fand kein geltendes erteiltes Patent, dessen Verfahren dieses Plugin nutzt, es sind aber Anmeldungen offen, die noch erteilt werden können. Weder dieses Plugin noch mbelib-neo noch die GPL geben eine Patentlizenz. Ob und wo du es baust und nutzt, entscheidest du; das hier ist keine Rechtsberatung. Deshalb gehört das Plugin nicht zu urfd-ng: dessen Setup und Builds nehmen es nur, wenn du es selbst nach `tcd/plugins/` klonst, und es ist aus, solange du es nicht lädst.

## Bauen

Braucht einen C/C++-Compiler, `make`, `cmake` (3.13 oder neuer), `tar` und `curl` oder `wget` (Debian, Ubuntu, Raspberry Pi OS; x86_64, arm64, armhf; auch macOS baut es).

```bash
git clone https://github.com/jcmerg/tcd-mbelib-neo-plugin.git
cd tcd-mbelib-neo-plugin
./build-mbelib-neo.sh        # lädt mbelib-neo 2.6.7 (gegen seine SHA-256 geprüft) und baut es hier
make
make check                   # lädt das Plugin wie tcd und lässt jeden Codec einmal laufen
sudo make install            # /usr/local/lib/tcd/mbelib_neo_codec.so
```

`CFLAGS`, `CXXFLAGS`, `CPPFLAGS` und `LDFLAGS` aus der Umgebung werden übernommen (Standard ist `-O2`), `make WERROR=1` macht Warnungen zu Fehlern, `make DESTDIR=… install` installiert in ein Staging-Verzeichnis. Das Plugin exportiert nur `tcd_codec_plugin`; die Symbole von mbelib-neo bleiben darin.

Oder dieses Repository nach `tcd/plugins/mbelib-neo` des urfd-ng-Klons klonen: Das tcd-Docker-Image und `tcd/build.sh` bauen es mit tcd und installieren es, `setup.sh update` holt es auf den neuen Stand (siehe „Codec-Plugins“ und „tcd in Docker“ in [tcd/README.de.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.de.md#codec-plugins)).

Zum Testen hat urfd-ng `tcd/tools/` (`make` dort): `./codectest /usr/local/lib/tcd/mbelib_neo_codec.so dmr|dstar|p25 ein.raw aus.frames aus.raw` kodiert und dekodiert 8-kHz-s16le-PCM und gibt die Zeit pro Frame aus; `dvsitest` vergleicht mit einem DV3003.

mbelib-neo liegt nicht in diesem Repository: `build-mbelib-neo.sh` holt sein Quell-Archiv, das Plugin linkt es statisch. Nur die Teile für D-Star, AMBE+2 und IMBE landen im Plugin. Auf 64-Bit-Systemen nutzt mbelib-neo seine SIMD-Routinen von selbst; auf 32-Bit-ARM (armhf, z. B. Raspberry Pi OS) schaltet das Skript seine NEON-Routinen ein, wenn die CPU NEON kann (Raspberry Pi 2 und neuer). Die Binaries bleiben übertragbar (`MBELIB_ENABLE_NATIVE=OFF`).

### mbelib-neo aktualisieren

Das Skript baut genau eine Version, gegen ihre SHA-256 geprüft, damit ein Build überall gleich ist. Für eine neuere Version von [mbelib-neo](https://github.com/arancormonk/mbelib-neo/releases):

```bash
VER=2.7.0   # die neue Version
curl -fsSL https://github.com/arancormonk/mbelib-neo/releases/download/v$VER/mbelib-neo-$VER-source.tar.gz | sha256sum
```

Version und Summe in `VER=` und `SUM=` in `build-mbelib-neo.sh` und die Version ins `Makefile` eintragen, das alte Verzeichnis `mbelib-neo-*` löschen, dann `./build-mbelib-neo.sh && make clean && make` (in Docker oder in `tcd/plugins/`: tcd neu bauen, das baut das Plugin mit). Vorher die Release-Notes lesen: Eine neue Hauptversion kann die API ändern.

`transcode` (`mbelib_neo_transcode.c`) nutzt die privaten Header und Quantisierer von mbelib-neo (`src/internal`, im statischen Archiv), die keine Version festschreibt. Deshalb baut das Plugin nur mit der Version, die `static_assert` in `mbelib_neo_plugin.cpp` (die installierte Bibliothek) und `MBELIB_PIN` im `Makefile` (der Quellbaum) nennen: Für eine neue Version `mbelib_neo_transcode.c` gegen deren `src/internal` prüfen (Quantisierer, Tabellen, `struct ambe_enc_frame`, das Packen der Frames), dann die Version an beiden Stellen eintragen und `make check` laufen lassen. `MBELIB_SRC=` nennt den Quellbaum von mbelib-neo, wenn er nicht neben `MBELIB=` liegt.

## Benutzen

In `/usr/local/etc/tcd.ini`, dann tcd neu starten:

```ini
CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
```

- **Kein AMBE-Stick**: tcd macht D-Star und DMR in Software, für beliebig viele `Modules`.
- **Ein Stick**: Der Stick macht D-Star, das Plugin DMR/YSF.
- **Zwei Sticks**: Die Sticks übernehmen die ersten Module aus `Modules`, das Plugin den Rest (DV3000 + DV3003 mit `Modules = FSABC`: F und S in Hardware, A, B und C in Software).
- `CodecDStar = mbelib-neo` / `CodecDMR = mbelib-neo` legen einen Codec von Anfang an in Software, auch wo ein Stick ihn könnte; das Web-Dashboard von tcd schaltet zur Laufzeit zwischen Sticks und Plugin um.
- `CodecP25 = mbelib-neo` (tcd 0.14.0 und neuer) macht P25 mit dem Plugin statt mit dem eigenen OP25-Vocoder von tcd; ohne die Zeile oder mit einem älteren tcd bleibt P25 bei OP25. Das Dashboard schaltet auch P25 zur Laufzeit um.
- Der Monitor von tcd zählt Bitfehler nur für D-Star und DMR: tcd gibt P25 als die 88 IMBE-Parameterbits weiter, ohne ihre FEC, deshalb hat das Plugin dort keine Fehler zu zählen (immer 0), und ein P25-Frame wird nur wiederholt, nach vier Wiederholungen in Folge stummgeschaltet, wenn seine Tonhöhe unbrauchbar ist.

- **Direkt transcodieren** (`Transcode = direct`, ab tcd 0.20.0 der Standard): Kommt ein Stream als D-Star, DMR/YSF oder P25 herein und geht als ein anderer der drei hinaus, beide über dieses Plugin, codiert das Plugin den ausgehenden Frame aus dem Modell des Vocoders (Grundfrequenz, Stimmhaftigkeit und Amplitude je Harmonischer) statt aus dem decodierten Audio. Die Harmonischen werden auf das Tonhöhenraster des Ziels gelegt, die Stimmhaftigkeit je Band nach Energie entschieden und die Amplituden mit dem Quantisierer des Ziels gegen dessen eigene Vorhersage kodiert; der Gain von tcd geht in die Amplituden ein. Tonframes (DTMF und Einzeltöne) werden zu Tonframes des Ziels, wo es sie hat, sonst (P25) zu stimmhaften Harmonischen auf den Tonfrequenzen; Wiederholungen spielen den letzten Frame weiter, Stummschaltungen werden Stille. `make check` prüft alle sechs Richtungen gegen den Audio-Weg (Pegel innerhalb von 3 dB).

Einzelheiten unter „Codec-Plugins“ in [tcd/README.de.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.de.md#codec-plugins).

## Gemessen

Auf einem Raspberry Pi 4 (armhf) etwa 0,18 ms zum Kodieren und 0,04 ms zum Dekodieren eines 20-ms-Frames mit den NEON-Routinen (0,20 und 0,07 ms ohne, die kodierten Frames Bit für Bit gleich), auf einem Pi 5 0,09 ms und 0,03 ms. Gegen einen DVSI DV3003 gehen die Frames in beide Richtungen: mbelib-neo-Frames, die der DV3003 dekodiert, und DV3003-Frames, die mbelib-neo dekodiert, kommen mit demselben Pegel und Hüllkurvenverlauf an wie beim DV3003 allein, für D-Star und DMR.

Klang gegen einen DV3003 (28.10.2026, mbelib-neo 2.6.5, Plugin 1.0.2; 1.0.3 bis 1.1.1 kodieren D-Star und DMR genauso): PESQ (ITU-T P.862, Schmalband, MOS von 1 bis 4,5) gegen das Original, Mittel über 19 Clips (18 FM-Mitschnitte, 1 Sprachclip, 8 kHz, je höchstens die ersten 100 s), jedes Paar aus Encoder und Decoder:

| Encoder → Decoder | DMR/YSF | D-Star |
|---|---|---|
| DV3003 → DV3003 | 2,52 | 2,48 |
| DV3003 → mbelib-neo | 2,50 | 2,40 |
| mbelib-neo → DV3003 | 2,49 | 2,59 |
| mbelib-neo → mbelib-neo | 2,46 | 2,50 |

Mit Bitfehlern in den DV3003-Frames (1 / 3 / 5 % der Bits gekippt), dekodiert vom DV3003 und von mbelib-neo: DMR 2,47 / 2,37 / 2,15 gegen 2,45 / 2,32 / 2,14, D-Star 2,44 / 2,33 / 2,07 gegen 2,33 / 2,17 / 1,99.

- **DMR**: Encoder und Decoder so gut wie der DV3003, auch mit Bitfehlern.
- **D-Star**: Der Encoder so gut wie der DV3003 oder etwas besser (das hören die Funkgeräte von tcd), der Decoder etwa 0,1 schlechter, auch mit Bitfehlern.
- **P25** (kein DVSI-Baustein kann IMBE, deshalb gegen den eigenen OP25-Vocoder von tcd, auf denselben Clips): OP25 → OP25 2,49, mbelib-neo → mbelib-neo 2,61, OP25 → mbelib-neo 2,53, mbelib-neo → OP25 2,55; über tcd nach DMR 2,20 gegen 2,26, nach M17 2,25 gegen 2,26. Ein kleiner Gewinn, vor allem vom Encoder; OP25 bleibt der Standard von tcd.
- 19 Clips und PESQ sind ein Anhaltspunkt, kein Hörtest: Unterschiede unter etwa 0,1 sagen wenig.

## Dateien

- `mbelib_neo_plugin.cpp`: das Plugin
- `mbelib_neo_transcode.c`, `mbelib_neo_transcode.h`: `transcode`, gegen die privaten Header von mbelib-neo 2.6.7
- `CodecPlugin.h`: eine Kopie der Plugin-Schnittstelle von tcd, gleich der von tcd gehalten (`make check` vergleicht sie in `tcd/plugins/`) (`TCD_CODEC_API 4`, ab tcd 0.20.0: `transcode`; tcd 0.15.0 bis 0.19.x brauchen Plugin 1.1.2, tcd 0.12.0 bis 0.14.x Plugin 1.0.3); `make TCD=/pfad/zu/urfd-ng/tcd` nimmt stattdessen die eines Klons
- `build-mbelib-neo.sh`: holt und baut mbelib-neo
- `plugin.map`: die Exportliste (Linux), `tests/smoke.cpp`: der Test von `make check`

## Versionen

- **1.2.0** (09.11.2026): tcd-Codec-API 4 (ab tcd 0.20.0): `transcode` für alle sechs Richtungen zwischen D-Star, DMR/YSF und P25, aus dem Modell des Vocoders statt aus dem decodierten Audio; Töne bleiben Töne, wo das Ziel sie hat. Nutzt die privaten Quantisierer von mbelib-neo 2.6.7 und baut nur mit dieser Version. Kodieren und Dekodieren unverändert.
- **1.1.2** (08.11.2026): mbelib-neo 2.6.7. Sein Decoder kommt näher an den von DVSI: die stimmhafte Phase, wie DVSI sie regeneriert (die Tonhöhe wechselt 16 Samples nach der Rahmenmitte, ein stimmhafter Einsatz startet die Phase neu), die Stimmhaftigkeit von DMR (Rate 33), wie DVSI sie liest, und DMR- und P25-Rahmen werden nicht mehr leiser skaliert, wo DVSI es nicht tut (bisher bis zu 2,95 dB leiser bei DMR, 1,05 dB bei P25). Die Klangmessungen unten sind noch die von 2.6.5. `CodecPlugin.h` wie in tcd 0.20.0 (API 4), das Plugin bleibt API 3.
- **1.1.1** (03.11.2026): Korrekturen aus einem Review; Kodieren und Dekodieren unverändert (Bit für Bit dieselben Frames und dasselbe PCM wie 1.1.0). Nur `tcd_codec_plugin` wird exportiert, aus `open` kommt keine Exception, ein Frame ist nur Nullen, wenn das Kodieren scheitert. `make check`, CI, `-Wall -Wextra`, `CXXFLAGS`/`LDFLAGS` aus der Umgebung, `WERROR=1`, `DESTDIR`. `build-mbelib-neo.sh` auch auf macOS, es prüft vorab seine Werkzeuge und baut neu, wenn `libmbe-neo.a` fehlt. Die P25-Statistik erklärt. `CodecPlugin.h` wie in tcd 0.16.0.
- **1.1.0** (31.10.2026): tcd-Codec-API 3 (ab tcd 0.15.0): Der Decoder meldet die korrigierten Bitfehler sowie wiederholte, stummgeschaltete, Ton- und Stille-Frames, die der Monitor von tcd zählt; die Frame-Formate werden angegeben. Kodieren und Dekodieren unverändert.
- **1.0.3** (28.10.2026): P25 IMBE 4400, Encoder und Decoder aus mbelib-neo (`CodecP25 = mbelib-neo`, tcd 0.14.0 und neuer); D-Star und DMR unverändert.
- **1.0.2** (28.10.2026): mbelib-neo 2.6.5 (der Encoder kodiert Brummen und Hintergrundrauschen in Pausen bei den tiefsten Tonhöhen nicht mehr als stimmhaft).
- **1.0.1** (27.10.2026): mbelib-neo 2.6.4 (Decoder näher am DVSI: verlorene Frames, Töne, D-Star-Bänder); die Version des Plugins im Log und Monitor von tcd.
- **1.0.0** (25.10.2026): tcd-Codec-API 2, mbelib-neo 2.6.2.

## Lizenz

GNU General Public License v3 oder später, siehe [LICENSE](LICENSE). mbelib-neo ist GPL-2.0-or-later mit ISC- und PFFFT-Teilen, siehe dessen Repository.

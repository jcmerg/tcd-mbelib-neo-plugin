# tcd-mbelib-neo-plugin

[English](README.md) | [Deutsch](README.de.md)

Ein Codec-Plugin für **tcd**, den Transcoder von [urfd-ng](https://github.com/jcmerg/urfd-ng): D-Star (AMBE 3600x2400) und DMR/YSF/NXDN (AMBE+2 3600x2450) in Software, Encoder und Decoder, aus [mbelib-neo](https://github.com/arancormonk/mbelib-neo). Damit transcodiert tcd ohne DVSI-Hardware oder mehr Module, als die AMBE-Sticks Kanäle haben. Dazu kann es P25 (IMBE 4400), als Wahl neben dem eigenen OP25-Vocoder von tcd; urfd-ng 4.31.0 und neuer geben auch YSF VW (Voice Wide, dasselbe IMBE) als P25 an tcd.

Seit 1.2.0 transcodiert es D-Star, DMR/YSF und P25 auch direkt ineinander (`transcode`, tcd 0.20.0 und neuer, `Transcode = direct`): Der ausgehende Frame entsteht aus den Parametern des Vocoders, die der Decoder aus dem eingehenden gelesen hat, ohne Synthese und zweite Analyse.

## Patenthinweis

**Bitte vor dem Bauen und Benutzen lesen.** Software-Umsetzungen von AMBE und AMBE+2 können Patente berühren. Die [Patentübersicht](https://github.com/arancormonk/mbelib-neo/blob/main/legal/patents.md) von mbelib-neo führt US 8,359,197 (AMBE+2 Halbrate) als aktiv, voraussichtlich bis 20.05.2028, und weitere offene Einträge. In Europa sind seine Gegenstücke (EP 1 465 158, EP 1 748 425) und die älteren DVSI-Patente nach 20 Jahren abgelaufen, spätestens am 26.03.2024; eine Suche in DVSIs europäischen Patenten und Anmeldungen (28.10.2026) fand kein geltendes erteiltes Patent, dessen Verfahren dieses Plugin nutzt, es sind aber Anmeldungen offen, die noch erteilt werden können. Weder dieses Plugin noch mbelib-neo noch die GPL geben eine Patentlizenz. Ob und wo du es baust und nutzt, entscheidest du; das hier ist keine Rechtsberatung. Deshalb gehört das Plugin nicht zu urfd-ng: dessen Setup und Builds nehmen es nur, wenn du es selbst nach `tcd/plugins/` klonst, und es ist aus, solange du es nicht lädst.

Zum direkten Transcodieren (`transcode`, seit 1.2.0): DVSI hatte das Umsetzen zwischen MBE-Vocodern auf Ebene der Parameter patentiert, als „Voice transcoder“ (US 7,634,399, dazu die Teilanmeldung US 7,957,963) und in der Familie „Interoperable vocoder“ (US 7,970,606, US 8,315,860, EP 1 420 390 mit DE 603 16 396, CA 2,447,735, JP 4,166,673). Laut Google Patents (abgerufen am 09.11.2026) sind alle abgelaufen, US 7,634,399 am 07.11.2025 und EP 1 420 390 am 07.11.2023. Neuere Anmeldungen von DVSI, die die Suche fand, betreffen nach ihrem Titel anderes (Tonerkennung, Fehlerkorrektur). `transcode` nutzt den Quantisierer von AMBE+2, für den US 8,359,197 oben weiter gilt. Auch das ist eine Recherche, keine vollständige Suche und keine Rechtsauskunft; der Status bei Google Patents ist keine amtliche Auskunft.

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

Zum Testen hat urfd-ng `tcd/tools/` (`make` dort): `./codectest /usr/local/lib/tcd/mbelib_neo_codec.so dmr|dstar|p25 ein.raw aus.frames aus.raw` kodiert und dekodiert 8-kHz-s16le-PCM und gibt die Zeit pro Frame aus; mit zwei Codecs (`dstar:dmr`, beliebige zwei aus `dstar`, `dmr`, `p25`) macht es die Frames des zweiten mit `transcode`, wie der direkte Weg von tcd; `dvsitest` vergleicht mit einem DV3003.

mbelib-neo liegt nicht in diesem Repository: `build-mbelib-neo.sh` holt sein Quell-Archiv, das Plugin linkt es statisch. Nur die Teile für D-Star, AMBE+2 und IMBE landen im Plugin. Auf 64-Bit-Systemen nutzt mbelib-neo seine SIMD-Routinen von selbst; auf 32-Bit-ARM (armhf, z. B. Raspberry Pi OS) schaltet das Skript seine NEON-Routinen ein, wenn die CPU NEON kann (Raspberry Pi 2 und neuer). Die Binaries bleiben übertragbar (`MBELIB_ENABLE_NATIVE=OFF`).

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

Direkt transcodiert (1.2.0, 09.11.2026, mbelib-neo 2.6.7): PESQ gegen das Original über drei Sprachclips (deutsch weiblich, englisch weiblich und männlich, 8 kHz), jeder Weg mit dem Plugin auf beiden Seiten; der Audio-Weg dekodiert und kodiert neu, wie tcd ohne `transcode` (beides ohne die AGC von tcd), gemessen mit `codectest` von tcd:

| Quelle → Ziel | Audio-Weg | direkt | Gewinn |
|---|---|---|---|
| D-Star → DMR | 2,56 | 2,80 | +0,24 |
| D-Star → P25 | 2,63 | 2,98 | +0,36 |
| DMR → D-Star | 2,54 | 2,81 | +0,27 |
| DMR → P25 | 2,60 | 2,90 | +0,30 |
| P25 → D-Star | 2,65 | 2,92 | +0,27 |
| P25 → DMR | 2,63 | 2,95 | +0,33 |

Jeder der 18 Fälle gewinnt (+0,16 bis +0,44). Der Pegel liegt etwa 1 dB unter dem des Audio-Wegs; `transcode` braucht ein Fünftel bis ein Fünfzehntel der Rechenzeit des Kodierens. Drei Clips sind weniger als die 19 oben, die Richtung ist aber in jedem Clip dieselbe.

## Dateien

- `mbelib_neo_plugin.cpp`: das Plugin
- `mbelib_neo_transcode.c`, `mbelib_neo_transcode.h`: `transcode`, gegen die privaten Header von mbelib-neo 2.6.7
- `CodecPlugin.h`: eine Kopie der Plugin-Schnittstelle von tcd, gleich der von tcd gehalten (`make check` vergleicht sie in `tcd/plugins/`) (`TCD_CODEC_API 4`, ab tcd 0.20.0: `transcode`; tcd 0.15.0 bis 0.19.x brauchen Plugin 1.1.2, tcd 0.12.0 bis 0.14.x Plugin 1.0.3); `make TCD=/pfad/zu/urfd-ng/tcd` nimmt stattdessen die eines Klons
- `build-mbelib-neo.sh`: holt und baut mbelib-neo
- `plugin.map`: die Exportliste (Linux), `tests/smoke.cpp`: der Test von `make check`

## Versionen

- **1.2.0** (09.11.2026): tcd-Codec-API 4 (ab tcd 0.20.0): `transcode` für alle sechs Richtungen zwischen D-Star, DMR/YSF und P25, aus dem Modell des Vocoders statt aus dem decodierten Audio; Töne bleiben Töne, wo das Ziel sie hat; +0,24 bis +0,36 PESQ gegenüber Decodieren und neu Codieren (siehe „Gemessen“). Nutzt die privaten Quantisierer von mbelib-neo 2.6.7 und baut nur mit dieser Version. Kodieren und Dekodieren unverändert.
- **1.1.2** (08.11.2026): mbelib-neo 2.6.7. Sein Decoder kommt näher an den von DVSI: die stimmhafte Phase, wie DVSI sie regeneriert (die Tonhöhe wechselt 16 Samples nach der Rahmenmitte, ein stimmhafter Einsatz startet die Phase neu), die Stimmhaftigkeit von DMR (Rate 33), wie DVSI sie liest, und DMR- und P25-Rahmen werden nicht mehr leiser skaliert, wo DVSI es nicht tut (bisher bis zu 2,95 dB leiser bei DMR, 1,05 dB bei P25). Die Klangmessungen unten sind noch die von 2.6.5. `CodecPlugin.h` wie in tcd 0.20.0 (API 4), das Plugin bleibt API 3.
- **1.1.1** (03.11.2026): Korrekturen aus einem Review; Kodieren und Dekodieren unverändert (Bit für Bit dieselben Frames und dasselbe PCM wie 1.1.0). Nur `tcd_codec_plugin` wird exportiert, aus `open` kommt keine Exception, ein Frame ist nur Nullen, wenn das Kodieren scheitert. `make check`, CI, `-Wall -Wextra`, `CXXFLAGS`/`LDFLAGS` aus der Umgebung, `WERROR=1`, `DESTDIR`. `build-mbelib-neo.sh` auch auf macOS, es prüft vorab seine Werkzeuge und baut neu, wenn `libmbe-neo.a` fehlt. Die P25-Statistik erklärt. `CodecPlugin.h` wie in tcd 0.16.0.
- **1.1.0** (31.10.2026): tcd-Codec-API 3 (ab tcd 0.15.0): Der Decoder meldet die korrigierten Bitfehler sowie wiederholte, stummgeschaltete, Ton- und Stille-Frames, die der Monitor von tcd zählt; die Frame-Formate werden angegeben. Kodieren und Dekodieren unverändert.
- **1.0.3** (28.10.2026): P25 IMBE 4400, Encoder und Decoder aus mbelib-neo (`CodecP25 = mbelib-neo`, tcd 0.14.0 und neuer); D-Star und DMR unverändert.
- **1.0.2** (28.10.2026): mbelib-neo 2.6.5 (der Encoder kodiert Brummen und Hintergrundrauschen in Pausen bei den tiefsten Tonhöhen nicht mehr als stimmhaft).
- **1.0.1** (27.10.2026): mbelib-neo 2.6.4 (Decoder näher am DVSI: verlorene Frames, Töne, D-Star-Bänder); die Version des Plugins im Log und Monitor von tcd.
- **1.0.0** (25.10.2026): tcd-Codec-API 2, mbelib-neo 2.6.2.

## Lizenz

GNU General Public License v3 oder später, siehe [LICENSE](LICENSE). mbelib-neo ist GPL-2.0-or-later mit ISC- und PFFFT-Teilen, siehe dessen Repository.

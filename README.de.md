# tcd-mbelib-neo-plugin

[English](README.md) | [Deutsch](README.de.md)

Ein Codec-Plugin für **tcd**, den Transcoder von [urfd-ng](https://github.com/jcmerg/urfd-ng): D-Star (AMBE 3600x2400) und DMR/YSF/NXDN (AMBE+2 3600x2450) in Software, Encoder und Decoder, aus [mbelib-neo](https://github.com/arancormonk/mbelib-neo). Damit transcodiert tcd ohne DVSI-Hardware oder mehr Module, als die AMBE-Sticks Kanäle haben.

## Patenthinweis

**Bitte vor dem Bauen und Benutzen lesen.** Software-Umsetzungen von AMBE und AMBE+2 können Patente berühren. Die [Patentübersicht](https://github.com/arancormonk/mbelib-neo/blob/main/legal/patents.md) von mbelib-neo führt US 8,359,197 (AMBE+2 Halbrate) als aktiv, voraussichtlich bis 20.05.2028, und weitere offene Einträge. Weder dieses Plugin noch mbelib-neo noch die GPL geben eine Patentlizenz. Ob und wo du es baust und nutzt, entscheidest du; das hier ist keine Rechtsberatung. Deshalb gehört das Plugin nicht zu urfd-ng, wird von dessen Setup und Docker-Image nicht gebaut und ist aus, solange du es nicht lädst.

## Bauen

Braucht einen C/C++-Compiler, `make`, `cmake` und `curl` oder `wget` (Debian, Ubuntu, Raspberry Pi OS; x86_64, arm64, armhf).

```bash
git clone https://github.com/jcmerg/tcd-mbelib-neo-plugin.git
cd tcd-mbelib-neo-plugin
./build-mbelib-neo.sh        # lädt mbelib-neo 2.6.2 (gegen seine SHA-256 geprüft) und baut es hier
make
sudo make install            # /usr/local/lib/tcd/mbelib_neo_codec.so
```

Für tcd in Docker dieses Repository stattdessen nach `tcd/plugins/mbelib-neo` des urfd-ng-Klons klonen; das Image baut es mit (siehe „tcd in Docker“ in [tcd/README.de.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.de.md#tcd-in-docker)).

mbelib-neo liegt nicht in diesem Repository: `build-mbelib-neo.sh` holt sein Quell-Archiv, das Plugin linkt es statisch. Nur die Teile für D-Star und AMBE+2 landen im Plugin. Auf 64-Bit-Systemen nutzt mbelib-neo seine SIMD-Routinen von selbst; auf 32-Bit-ARM (armhf, z. B. Raspberry Pi OS) schaltet das Skript seine NEON-Routinen ein, wenn die CPU NEON kann (Raspberry Pi 2 und neuer). Die Binaries bleiben übertragbar (`MBELIB_ENABLE_NATIVE=OFF`).

### mbelib-neo aktualisieren

Das Skript baut genau eine Version, gegen ihre SHA-256 geprüft, damit ein Build überall gleich ist. Für eine neuere Version von [mbelib-neo](https://github.com/arancormonk/mbelib-neo/releases):

```bash
VER=2.7.0   # die neue Version
curl -fsSL https://github.com/arancormonk/mbelib-neo/releases/download/v$VER/mbelib-neo-$VER-source.tar.gz | sha256sum
```

Version und Summe in `VER=` und `SUM=` in `build-mbelib-neo.sh` eintragen, das alte Verzeichnis `mbelib-neo-*` löschen, dann `./build-mbelib-neo.sh && make clean && make` (in Docker: das tcd-Image neu bauen). Vorher die Release-Notes lesen: Eine neue Hauptversion kann die API ändern.

## Benutzen

In `/usr/local/etc/tcd.ini`, dann tcd neu starten:

```ini
CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
```

- **Kein AMBE-Stick**: tcd macht D-Star und DMR in Software, für beliebig viele `Modules`.
- **Ein Stick**: Der Stick macht D-Star, das Plugin DMR/YSF.
- **Zwei Sticks**: Die Sticks übernehmen die ersten Module aus `Modules`, das Plugin den Rest (DV3000 + DV3003 mit `Modules = FSABC`: F und S in Hardware, A, B und C in Software).
- `CodecDStar = mbelib-neo` / `CodecDMR = mbelib-neo` legen einen Codec von Anfang an in Software, auch wo ein Stick ihn könnte; das Web-Dashboard von tcd schaltet zur Laufzeit zwischen Sticks und Plugin um.

Einzelheiten unter „Codec-Plugins“ in [tcd/README.de.md](https://github.com/jcmerg/urfd-ng/blob/main/tcd/README.de.md#codec-plugins).

## Gemessen

Auf einem Raspberry Pi 4 (armhf) etwa 0,18 ms zum Kodieren und 0,04 ms zum Dekodieren eines 20-ms-Frames mit den NEON-Routinen (0,20 und 0,07 ms ohne, die kodierten Frames Bit für Bit gleich), auf einem Pi 5 0,09 ms und 0,03 ms. Gegen einen DVSI DV3003 gehen die Frames in beide Richtungen: mbelib-neo-Frames, die der DV3003 dekodiert, und DV3003-Frames, die mbelib-neo dekodiert, kommen mit demselben Pegel und Hüllkurvenverlauf an wie beim DV3003 allein, für D-Star und DMR.

Klang gegen einen DV3003 (25.10.2026): PESQ (ITU-T P.862, Schmalband, MOS von 1 bis 4,5) gegen das Original, Mittel über 5 Clips (4 FM-Mitschnitte, 1 Sprachclip, 8 kHz), jedes Paar aus Encoder und Decoder:

| Encoder → Decoder | DMR/YSF | D-Star |
|---|---|---|
| DV3003 → DV3003 | 2,66 | 2,62 |
| DV3003 → mbelib-neo | 2,56 | 2,46 |
| mbelib-neo → DV3003 | 2,64 | 2,40 |
| mbelib-neo → mbelib-neo | 2,61 | 2,59 |

Mit Bitfehlern in den DV3003-Frames (1 / 3 / 5 % der Bits gekippt), dekodiert vom DV3003 und von mbelib-neo: DMR 2,63 / 2,46 / 2,20 gegen 2,50 / 2,39 / 2,15, D-Star 2,56 / 2,34 / 2,11 gegen 2,41 / 2,25 / 1,92.

- **DMR**: Der Encoder ist so gut wie der DV3003 (das hören die Funkgeräte von tcd), der Decoder etwa 0,1 MOS schlechter; hörbar höchstens im direkten A/B-Vergleich.
- **D-Star**: etwa 0,15 schlechter im Decoder und 0,2 im Encoder. Wo ein AMBE-Stick da ist, D-Star auf ihm lassen und das Plugin DMR und die Module über die Sticks hinaus machen lassen.
- Mit Bitfehlern verliert mbelib-neo etwa so schnell wie der DV3003.
- Fünf Clips und PESQ sind ein Anhaltspunkt, kein Hörtest: Unterschiede unter etwa 0,1 sagen wenig.

## Dateien

- `mbelib_neo_plugin.cpp`: das Plugin
- `CodecPlugin.h`: eine Kopie der Plugin-Schnittstelle von tcd (`TCD_CODEC_API 1`); `make TCD=/pfad/zu/urfd-ng/tcd` nimmt stattdessen die eines Klons
- `build-mbelib-neo.sh`: holt und baut mbelib-neo

## Lizenz

GNU General Public License v3 oder später, siehe [LICENSE](LICENSE). mbelib-neo ist GPL-2.0-or-later mit ISC- und PFFFT-Teilen, siehe dessen Repository.

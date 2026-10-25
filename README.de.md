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

mbelib-neo liegt nicht in diesem Repository: `build-mbelib-neo.sh` holt sein Quell-Archiv, das Plugin linkt es statisch. Nur die Teile für D-Star und AMBE+2 landen im Plugin.

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

Auf einem Raspberry Pi 4 (armhf) etwa 0,20 ms zum Kodieren und 0,08 ms zum Dekodieren eines 20-ms-Frames, auf einem Pi 5 0,09 ms und 0,03 ms. Gegen einen DVSI DV3003 gehen die Frames in beide Richtungen: mbelib-neo-Frames, die der DV3003 dekodiert, und DV3003-Frames, die mbelib-neo dekodiert, kommen mit demselben Pegel und Hüllkurvenverlauf an wie beim DV3003 allein, für D-Star und DMR.

## Dateien

- `mbelib_neo_plugin.cpp`: das Plugin
- `CodecPlugin.h`: eine Kopie der Plugin-Schnittstelle von tcd (`TCD_CODEC_API 1`); `make TCD=/pfad/zu/urfd-ng/tcd` nimmt stattdessen die eines Klons
- `build-mbelib-neo.sh`: holt und baut mbelib-neo

## Lizenz

GNU General Public License v3 oder später, siehe [LICENSE](LICENSE). mbelib-neo ist GPL-2.0-or-later mit ISC- und PFFFT-Teilen, siehe dessen Repository.

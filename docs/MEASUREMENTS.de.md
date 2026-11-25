# Messungen

[English](MEASUREMENTS.md) | [Deutsch](MEASUREMENTS.de.md)

Geschwindigkeit und Klang des Plugins, gemessen mit den in jedem Teil genannten Versionen. Die [README](../README.de.md) hat eine kurze Zusammenfassung.

Auf einem Raspberry Pi 4 (armhf) etwa 0,18 ms zum Kodieren und 0,04 ms zum Dekodieren eines 20-ms-Frames mit den NEON-Routinen (0,20 und 0,07 ms ohne, die kodierten Frames Bit für Bit gleich), auf einem Pi 5 0,09 ms und 0,03 ms. Gegen einen DVSI DV3003 gehen die Frames in beide Richtungen: mbelib-neo-Frames, die der DV3003 dekodiert, und DV3003-Frames, die mbelib-neo dekodiert, kommen mit demselben Pegel und Hüllkurvenverlauf an wie beim DV3003 allein, für D-Star und DMR.

Encoder von mbelib-neo 2.6.12 (Plugin 1.3.1, 25.11.2026), PESQ auf den unten beschriebenen 19 Clips gegen einen DV3003, 2.6.11 (Plugin 1.3.0) zum Vergleich. 2.6.12 sendet einen Frame ohne stimmhaftes Band mit einem festen Pitch-Code (D-Star b0 123, DMR 119, P25 127) und wählt die sechs Spektralvektoren von D-Star und DMR gemeinsam:

| Encoder → Decoder | DV3003 allein | 2.6.11 | 2.6.12 |
|---|---|---|---|
| DMR → DV3003 | 2,52 | 2,49 | 2,65 |
| DMR → mbelib-neo | | 2,50 | 2,65 |
| D-Star → DV3003 | 2,48 | 2,59 | 2,71 |
| D-Star → mbelib-neo | | 2,56 | 2,70 |

Die Decoder sind für D-Star unverändert, für DMR innerhalb von 0,02 (DV3003-Frames, auch mit 1 / 3 / 5 % Bitfehlern). Weißes Rauschen bei -30 und -50 dBFS, mit den festen Pitch-Codes kodiert, spielt der DV3003 als Rauschen mit demselben Pegel und ohne mehr Energie unter 150 Hz als beim verfolgten Pitch.

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

Decoder von mbelib-neo 2.6.8 (Plugin 1.2.1, 10.11.2026), PESQ gegen das Original: saubere Sprache (TTS, zwei Sätze mit je 8 Clips, deutsch und englisch) und 18 FM-Aufnahmen, Rahmen vom Encoder des DV3003, decodiert vom Plugin; 2.6.7 zum Vergleich. Mit 2.6.8 folgt das decodierte Audio eng dem, was der DV3003 selbst decodiert (Korrelation der Wellenform pro Rahmen 0,88 bis 0,94, mit 2.6.7 0,28 bis 0,77):

| Satz | 2.6.7 | 2.6.8 |
|---|---|---|
| D-Star, saubere Sprache (Satz 1 / Satz 2) | 2,87 / 2,47 | 3,03 / 2,59 |
| DMR, saubere Sprache (Satz 1 / Satz 2) | 2,75 / 2,36 | 2,91 / 2,51 |
| D-Star, FM | 2,38 | 2,48 |
| DMR, FM | 2,51 | 2,55 |
| P25 (Rahmen eines IMBE-Encoders in Software), FM | 2,64 | 2,75 |

Mit Bitfehlern (1 / 3 / 5 %) ist der Gewinn kleiner: D-Star FM +0,10 / +0,12 / +0,08, DMR FM +0,04 / +0,02 / +0,04.

Direkt transcodiert (1.2.1, 10.11.2026): PESQ gegen das Original über drei Sprachclips (de_anna, en_samantha, en_daniel aus dem TTS-Satz, 8 kHz), jeder Weg mit dem Plugin auf beiden Seiten; der Audio-Weg dekodiert und kodiert neu, wie tcd ohne `transcode` (beides ohne die AGC von tcd), gemessen mit `codectest` von tcd:

| Quelle → Ziel | Audio-Weg 2.6.7 | direkt 2.6.7 | Audio-Weg 2.6.8 | direkt 2.6.8 |
|---|---|---|---|---|
| D-Star → DMR | 2,67 | 2,97 | 2,73 | 3,10 |
| D-Star → P25 | 2,72 | 3,01 | 2,82 | 3,28 |
| DMR → D-Star | 2,66 | 2,94 | 2,71 | 3,10 |
| DMR → P25 | 2,72 | 3,04 | 2,83 | 3,27 |
| P25 → D-Star | 2,72 | 3,01 | 2,80 | 3,18 |
| P25 → DMR | 2,72 | 3,04 | 2,80 | 3,20 |

Der direkte Weg gewinnt mit 2.6.8 +0,37 bis +0,46 gegenüber dem Audio-Weg (mit 2.6.7 +0,28 bis +0,32); jeder der 18 Fälle gewinnt (+0,23 bis +0,58 mit 2.6.8). Der Pegel liegt etwa 1 dB unter dem des Audio-Wegs; `transcode` braucht ein Fünftel bis ein Fünfzehntel der Rechenzeit des Kodierens. Drei Clips sind weniger als die 19 oben, die Richtung ist aber in jedem Clip dieselbe.


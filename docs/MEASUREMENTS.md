# Measurements

[English](MEASUREMENTS.md) | [Deutsch](MEASUREMENTS.de.md)

Speed and sound of the plugin, measured with the versions named in each part. The [README](../README.md) has a short summary.

On a Raspberry Pi 4 (armhf) about 0.18 ms to encode and 0.04 ms to decode a 20 ms frame with the NEON routines (0.20 and 0.07 ms without, the frames encoded bit for bit the same), on a Pi 5 0.09 ms and 0.03 ms. Against a DVSI DV3003, frames go both ways: mbelib-neo frames decoded by the DV3003 and DV3003 frames decoded by mbelib-neo come out at the same level and envelope as the DV3003 on its own, for D-Star and DMR.

Encoders of mbelib-neo 2.6.12 (plugin 1.3.1, 2026-11-25), PESQ on the 19 clips described below against a DV3003, 2.6.11 (plugin 1.3.0) for comparison. 2.6.12 sends a frame with no voiced band at one fixed pitch code (D-Star b0 123, DMR 119, P25 127) and chooses the six spectral vectors of D-Star and DMR together:

| Encoder → decoder | DV3003 on its own | 2.6.11 | 2.6.12 |
|---|---|---|---|
| DMR → DV3003 | 2.52 | 2.49 | 2.65 |
| DMR → mbelib-neo | | 2.50 | 2.65 |
| D-Star → DV3003 | 2.48 | 2.59 | 2.71 |
| D-Star → mbelib-neo | | 2.56 | 2.70 |

The decoders are unchanged for D-Star and within 0.02 for DMR (DV3003 frames, also with 1 / 3 / 5 % bit errors). White noise at -30 and -50 dBFS, coded at the fixed pitch codes, plays on the DV3003 as noise at the same level and with no more energy below 150 Hz than at the tracked pitch.

Sound against a DV3003 (2026-10-28, mbelib-neo 2.6.5, plugin 1.0.2; 1.0.3 to 1.1.1 code D-Star and DMR the same): PESQ (ITU-T P.862, narrowband, MOS from 1 to 4.5) against the original audio, mean over 19 clips (18 FM recordings, 1 speech clip, 8 kHz, at most the first 100 s of each), each pair of encoder and decoder:

| Encoder → decoder | DMR/YSF | D-Star |
|---|---|---|
| DV3003 → DV3003 | 2.52 | 2.48 |
| DV3003 → mbelib-neo | 2.50 | 2.40 |
| mbelib-neo → DV3003 | 2.49 | 2.59 |
| mbelib-neo → mbelib-neo | 2.46 | 2.50 |

With bit errors in the DV3003 frames (1 / 3 / 5 % of the bits flipped), decoded by the DV3003 and by mbelib-neo: DMR 2.47 / 2.37 / 2.15 against 2.45 / 2.32 / 2.14, D-Star 2.44 / 2.33 / 2.07 against 2.33 / 2.17 / 1.99.

- **DMR**: encoder and decoder as good as the DV3003, also with bit errors.
- **D-Star**: the encoder as good as the DV3003 or a little better (what radios hear from tcd), the decoder about 0.1 lower, also with bit errors.
- **P25** (no DVSI part does IMBE, so against tcd's own OP25 vocoder, on the same clips): OP25 → OP25 2.49, mbelib-neo → mbelib-neo 2.61, OP25 → mbelib-neo 2.53, mbelib-neo → OP25 2.55; through tcd to DMR 2.20 against 2.26, to M17 2.25 against 2.26. A small gain, mostly from the encoder; OP25 stays tcd's default.
- 19 clips and PESQ are a guide, not a listening test: differences under about 0.1 say little.

Decoder of mbelib-neo 2.6.8 (plugin 1.2.1, 2026-11-10), PESQ against the original: clean speech (TTS, two sets of 8 clips, German and English) and 18 FM recordings, frames of the DV3003 encoder, decoded by the plugin; 2.6.7 for comparison. With 2.6.8 the decoded audio follows the DV3003's own decode closely (waveform correlation per frame 0.88 to 0.94, against 0.28 to 0.77 with 2.6.7):

| Set | 2.6.7 | 2.6.8 |
|---|---|---|
| D-Star, clean speech (set 1 / set 2) | 2.87 / 2.47 | 3.03 / 2.59 |
| DMR, clean speech (set 1 / set 2) | 2.75 / 2.36 | 2.91 / 2.51 |
| D-Star, FM | 2.38 | 2.48 |
| DMR, FM | 2.51 | 2.55 |
| P25 (frames of a software IMBE encoder), FM | 2.64 | 2.75 |

With bit errors (1 / 3 / 5 %) the gain is smaller: D-Star FM +0.10 / +0.12 / +0.08, DMR FM +0.04 / +0.02 / +0.04.

Transcoded directly (1.2.1, 2026-11-10): PESQ against the original over three speech clips (de_anna, en_samantha, en_daniel of the TTS set, 8 kHz), each path with the plugin on both sides; the audio path decodes and encodes again, as tcd without `transcode` (both without tcd's AGC), measured with tcd's `codectest`:

| Source → target | audio path 2.6.7 | direct 2.6.7 | audio path 2.6.8 | direct 2.6.8 |
|---|---|---|---|---|
| D-Star → DMR | 2.67 | 2.97 | 2.73 | 3.10 |
| D-Star → P25 | 2.72 | 3.01 | 2.82 | 3.28 |
| DMR → D-Star | 2.66 | 2.94 | 2.71 | 3.10 |
| DMR → P25 | 2.72 | 3.04 | 2.83 | 3.27 |
| P25 → D-Star | 2.72 | 3.01 | 2.80 | 3.18 |
| P25 → DMR | 2.72 | 3.04 | 2.80 | 3.20 |

The direct path gains +0.37 to +0.46 over the audio path with 2.6.8 (+0.28 to +0.32 with 2.6.7); each of the 18 cases gains (+0.23 to +0.58 with 2.6.8). The level is about 1 dB below the audio path's; `transcode` takes a fifth to a fifteenth of the time encoding takes. Three clips are fewer than the 19 above, but the direction is the same in every clip.


# Draft pedal physical findings — 2026-09-11

These notes capture the first physical listening pass on three source-only
reference drafts using the accepted 48 kHz / 1x CircuitPedal live path.

## EQD Tentacle

Observed:

- extremely low response at normal interface input gain;
- interface input had to be driven close to maximum to obtain a useful effect;
- physical behaviour was therefore not accepted.

Reference review found a high-confidence transcription error in the audio-path
output load. The model used `R_OUTPUT_LOAD = 4k7`, but the PedalPCB Squidward
reference lists audio-path `R13 = 47k`. The separate `R100 = 4k7` value is not
the final audio output load.

Action: corrected the model to `47k`. Retest required.

## BearFoot Blueberry Bass Overdrive

Observed:

- audio passed normally;
- only a minor tonal change;
- DRIVE at 100% produced very little overdrive;
- physical behaviour was therefore not accepted.

Reference review found a high-confidence factor-of-ten transcription error in
the JFET output stage. The model used `R10 = 5k6` from VA to the 2N5457 drain,
while the cross-checked Aion Procyon/Blueberry schematic shows `R10 = 56k`.

Action: corrected the model to `56k`. Retest required.

## Human Gear Animato

Observed:

- hard strums still clipped incorrectly;
- BOOST appeared to operate in the opposite direction;
- DISTORTION was overly sensitive;
- low DISTORTION positions became excessively quiet;
- TONE seemed broadly plausible but was difficult to judge around the other
  gain-staging problems.

Action: no circuit values changed in this pass. The Animato requires a dedicated
trace/control-orientation review rather than speculative tuning.

## Promotion rule

None of these three models should be added back to the stable bundled pedal
library until a repeat physical listening pass accepts their behaviour.

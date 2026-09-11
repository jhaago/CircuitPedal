# Draft pedal physical findings — 2026-09-11

These notes capture the first physical listening passes on three source-only
reference drafts using the accepted 48 kHz / 1x CircuitPedal live path.

## EQD Tentacle

First live test:

- extremely low response at normal interface input gain;
- interface input had to be driven close to maximum to obtain a useful effect;
- physical behaviour was therefore not accepted.

Reference review found that the model used `R_OUTPUT_LOAD = 4k7`, while the
PedalPCB Squidward reference lists audio-path `R13 = 47k`. The separate
`R100 = 4k7` value belongs to the utility/supply section. The model was corrected
to `47k` and physically retested.

Second live test:

- no meaningful audible improvement from the 47k correction;
- the pedal remained far too quiet at the same interface settings.

A subsequent automated signal-level campaign showed that the analogue model is
actually producing the expected octave path at realistic guitar-scale inputs:
at a 50 mV peak, 440 Hz input the model produces roughly 36 mV RMS analogue
output with the 880 Hz second harmonic strongly dominant and zero convergence
failures. The remaining large level loss was traced to model I/O calibration:
input is defined as `0.20 V` per digital full-scale while the draft used only
`1` digital full-scale per output volt. That creates about a fivefold digital
attenuation unrelated to the analogue circuit itself.

Action: `validation/tentacle-signal-level` contains a candidate `OUTPUT OUT 5`
calibration plus the signal-level campaign. Physical retest is required before
that calibration is promoted to `main`.

## BearFoot Blueberry Bass Overdrive

First live test:

- audio passed normally;
- only a minor tonal change;
- DRIVE at 100% produced very little overdrive;
- physical behaviour was therefore not accepted.

An initial reference review incorrectly concluded that the JFET drain resistor
should be `56k` rather than `5k6`. That change was physically retested and made
the model worse: it produced no useful output.

Re-checking the current Aion Procyon V2 documentation showed that `R10 = 5k6`
is correct and the Blueberry substitution list does not change it. The mistaken
`56k` conclusion came from mixing resistor numbering from an older schematic
revision.

Action: the model has been restored to `R10 = 5k6`, which returns it to the last
audible state. This is only a regression rollback, not a fidelity fix. The weak
overdrive behaviour still requires a dedicated topology/gain-stage validation
pass before promotion.

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

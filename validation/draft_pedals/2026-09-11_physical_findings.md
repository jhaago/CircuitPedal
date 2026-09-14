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

The first correction campaign remained based on the different Mad
Professor/Procyon version and therefore produced a hybrid rather than an
accurate BJFE Blueberry. The supplied BJFE #045 trace resolves the discrepancies.

Action: the source-only candidate has been rebuilt around the traced 370k Drive,
1M input pulldown, 14k7 op-amp input resistor, 22n JFET coupling capacitor,
BF244A output device and 2k6 source resistor. The incorrect JFET-source Tone
branch has been removed; Tone now connects its 22u side to the op-amp feedback
node and its 2n2 side to the final signal node, matching both the schematic and
the tracer's explanation that BJFE Tone affects output treble response.

The live-silence report exposed a separate model-I/O calibration error:
`AUDIO ... 0.20` was paired with `OUTPUT ... 0.5`, attenuating the analogue
result by 10x relative to unity conversion. Output is now calibrated at 5
digital full scale per volt. Automated 48 kHz / 1x validation checks Drive
range, distortion, solver convergence, usable BF244A DC bias and audible output
at a 2 mV input. This remains a candidate fidelity fix;
physical listening acceptance is required before promotion.

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

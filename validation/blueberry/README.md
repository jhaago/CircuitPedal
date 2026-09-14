# Blueberry Bass Overdrive validation

The Blueberry reference draft remains source-only until it passes a repeat
physical listening test. The 2026-09-14 candidate corrects three values against
the current Aion FX Procyon V2 trace and its explicit Blueberry substitutions:

- Drive: 250k audio to 500k audio
- R2: 14k7 to 15k
- JFET source resistor R11: 2k6 to 2k2

The earlier R10 experiment is not part of this candidate. R10 remains 5k6; the
temporary 56k value was a schematic-revision mix-up and silenced the JFET stage.

## Automated acceptance guard

`circuit_file_test` exercises the model through the production 48 kHz / 1x
solver at a 100 mV peak equivalent input. It requires:

- no nonlinear-solver failures at minimum or maximum Drive;
- maximum-Drive RMS output at least four times minimum-Drive output;
- a substantially clean minimum-Drive result; and
- useful third-harmonic generation at maximum Drive.

This guard proves that the corrected Drive network actually changes gain and
reaches nonlinear clipping. It does not replace a bass/interface listening test.

## Physical retest

Use the same interface gain and bass used for the rejected 2026-09-11 pass.
Check minimum, midpoint and maximum Drive with Tone centred and Volume matched.
Do not add the model to the bundled library unless Drive moves progressively
from lightly coloured/clean response into audible overdrive without an abnormal
level drop, gating or hard-strum instability.

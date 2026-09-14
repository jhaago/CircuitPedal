# Blueberry Bass Overdrive validation

The Blueberry reference draft remains source-only until it passes a repeat
physical listening test. The 2026-09-14 second candidate is reconstructed
directly from the supplied BJFE Blueberry Bass Overdrive #045 trace rather than
from the different Mad Professor/Procyon topology.

## Corrected BJFE trace

The candidate now uses the traced values and connections:

- CA3130EZ op-amp with no input JFET stage;
- 1M input pulldown, 22n input capacitor, 360k bias feed and 14k7 input resistor;
- A370k Drive pot with antiparallel red LEDs;
- antiparallel 1N4007 post-clamp plus the retained 27k/22n and 147k/4n7
  frequency-dependent feedback branches;
- 22n coupling into a BF244A output JFET, with 5k6 drain and 2k6 source
  resistors;
- A50k Tone pot with its wiper grounded, its 22u side connected to the op-amp
  1k/220n feedback node, and its 2n2 side connected to the final signal node;
- B50k linear Volume control;
- `OUTPUT OUT 5` calibration, matching the reciprocal of the 0.20 V/full-scale
  input calibration so unity analogue voltage remains unity digital level.

The earlier candidate incorrectly combined Procyon component substitutions
with the BJFE identity. Most importantly, it used a 500k Drive, 2N5457/2k2
output stage, and an invented JFET-source Tone branch. Those changes are removed.

## BF244A compact model

CircuitPedal now recognises BF244A as a named N-JFET. The compact model uses
4.0 mA IDSS and -2.5 V pinch-off as a representative A-grade device. The
manufacturer datasheet specifies 2.0-6.5 mA IDSS for BF244A; actual vintage
device spread is not represented until a transistor from a reference unit can
be measured.

## Automated acceptance guard

`circuit_file_test` exercises the model through the production 48 kHz / 1x
solver at a 20 mV peak equivalent input, with an additional 2 mV low-level
check. It requires:

- the BF244A model to remain inside the documented A-grade IDSS range;
- the output JFET source and drain to settle into usable self-bias/headroom;
- clean output to reach at least dry level at 2 mV input and maximum Drive to
  exceed dry level by at least five times;
- no nonlinear-solver failures at minimum or maximum Drive;
- maximum-Drive RMS output at least four times minimum-Drive output;
- a substantially clean minimum-Drive result; and
- useful third-harmonic generation at maximum Drive.

The previous `OUTPUT OUT 0.5` value attenuated the entire analogue result by
10x relative to the input conversion. It could pass a 100 mV bench stimulus
while becoming effectively inaudible with normal interface levels.

These guards confirm that the traced circuit is operational in the solver.
They do not replace a bass/interface listening test.

## Physical retest

Load this source-only draft externally and use the same interface gain and bass
used for the rejected 2026-09-11 pass. Check minimum, midpoint and maximum Drive
with Tone centred and Volume matched, then sweep Tone at low and high Drive.
Do not add the model to the bundled stable library unless Drive progresses from
light colour into audible overdrive, Tone behaves as a treble-response control,
and there is no abnormal level drop, gating or hard-strum instability.

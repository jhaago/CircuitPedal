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
at a 2 mV input. The 2026-09-14 physical retest confirmed working output, and
the Blueberry was promoted to the bundled pedal list.

## Human Gear Animato

First physical pass observed:

- hard strums appeared to clip incorrectly;
- BOOST appeared to operate in the opposite direction;
- DISTORTION was overly sensitive;
- low DISTORTION positions became excessively quiet;
- TONE seemed broadly plausible but was difficult to judge around the other
  gain-staging problems.

Dedicated review and 48 kHz / 1x diagnostics on 2026-09-15 found one confirmed
model fault: the two outer terminals of the internal 10k BOOST trimmer were
reversed relative to CircuitPedal's normalized pot convention. BOOST 0 had been
connected to the Rangemaster signal end and BOOST 1 to the ideal 9 V AC-ground
rail, exactly explaining the observed backwards action. The terminals were
swapped without changing value, taper or default position.

The other reported behaviours were investigated before changing them:

- **DISTORTION sensitivity:** the trace/Aion schematic specifies a physical dual
  100k audio-taper control. Both gangs are attenuators and move together. The
  model measures about -70.8 dB relative to maximum at 10% rotation, -18.7 dB at
  25%, -2.5 dB at 50%, -0.5 dB at 75%, and 0 dB at 100%. This is steep, but it
  follows the traced dual-audio-pot topology; it was not linearized speculatively.
- **Low-drive level:** the analogue/output-domain calibration is healthy. At
  medium drive a 50 mV-peak analogue input produces 0.4106 V RMS at the circuit
  OUT node and 0.2053 FS RMS at the host output, exactly matching the configured
  0.5 FS/V conversion. The low control positions are therefore a control-law/
  circuit effect rather than the Blueberry-style output-calibration bug.
- **Hard-input clipping:** a 150 mV-peak sine at high drive reached 0.5901 FS peak
  with 0% host full-scale clipping and zero solver failures. Step, impulse and
  two dual-tone stress cases likewise produced 0% host clipping and zero solver
  failures. A physical hard-strum retest is still needed because these controlled
  signals cannot reproduce every pickup/transient interaction.
- **Tone:** low-level measurements confirm the expected Big-Muff-style pan. At
  5 kHz, Tone 0 measured -8.44 dB gain and Tone 1 +10.02 dB; at 110 Hz the same
  movement changes +18.46 dB to +10.80 dB.
- **Bass response:** the model intentionally retains the traced 10 nF Rangemaster
  input coupling network. Low-level output gain rises from -9.17 dB at 40 Hz to
  +8.00 dB at 82 Hz, +24.35 dB at 196 Hz and +30.53 dB at 440 Hz. Aion explicitly
  notes that the Animato topology cuts bass and that bass users often restore it
  externally with a clean blend, so no internal bass-preservation feature was
  added.
- **Sample rate/stability:** 44.1, 48 and 96 kHz matched closely in the focused
  220 Hz test (RMS ≈0.2428 FS, THD ≈30.1%), with zero solver failures.

Action: the BOOST orientation fix and focused acceptance campaign live on the
`validation/animato` branch. The Animato remains withheld from the stable bundle
pending a second physical listening pass; no unrelated pedal or shared DSP code
was changed.

## Promotion rule

Blueberry passed its 2026-09-14 output retest and is now bundled. EQD Tentacle
and Human Gear Animato remain withheld until repeat physical listening accepts
their behaviour.

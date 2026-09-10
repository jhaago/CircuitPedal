# Boing Reverb reference targets

## Scope

Target a one-knob J. Rockett Boing-style reverb built around a TL072 analogue
signal path and an Accutronics/Belton BTDR-2H reverb module.

The user-supplied Effects Layouts drawing is useful for board/component tracing,
but the source page labels that layout **Unverified**. Do not treat that image
alone as the electrical truth source. Cross-check the signal path against an
independent Boing-style schematic before translating it into the generic MNA
circuit format.

## Cross-checked analogue parts

The independent Five Cats "Boing Boing" schematic identifies the following
major signal-path parts:

- TL072 dual op-amp signal stages;
- BTDR-2H reverb module;
- REVERB: A10k;
- R4 33k;
- C4 15nF;
- R5 10k;
- R6 22k;
- R7 10k;
- R8 39k;
- R9 39k;
- R10 10k;
- R11 390 ohm;
- R12 100k;
- C8 4.7uF;
- R13 15k;
- 78L05 / 5 V regulator supply for the reverb brick.

These values also line up with the labels visible in the user-supplied board
layout sufficiently well to use the independent schematic as the initial
translation reference.

## BTDR-2 published electrical envelope

The BTDR-2 family is itself a digital black-box component, so CircuitPedal
should model it as a component inside the surrounding analogue circuit rather
than pretending the whole pedal is one generic reverb algorithm.

Published BTDR-2 targets used by the first behavioural model:

- supply: nominal 5.0 V;
- maximum input: approximately 1.5 V peak;
- input impedance: approximately 10 kohm;
- output impedance: approximately 220 ohm;
- nominal gain: approximately -3 dB per output;
- stereo/decorrelated outputs may be summed to mono;
- short T60: approximately 2.0 s;
- medium T60: approximately 2.5 s;
- long T60: approximately 2.85 s.

The exact BTDR-2 decay suffix used by the original/reference Boing still needs
confirmation. The first CircuitPedal vertical slice therefore defaults to the
medium 2.5 s behavioural model while exposing all three variants in the BTDR-2
component API.

## Product behaviour target

The Boing control should increase or decrease reverb amount while retaining the
dry signal. The intended character is a simple, spring-inspired ambience with
substantial headroom rather than an intentionally overdriven reverb path.

## Implementation stages

1. Reusable BTDR-2 behavioural component and automated tests.
2. Boing reference processor for quick offline validation.
3. TL072 named device alias in the generic device library.
4. Generic circuit-file BTDR-2 primitive.
5. Translate the surrounding Boing R/C/TL072 network into a `.cpedal` file.
6. Add live GUI loading and the single REVERB control.
7. Physical listening comparison and, ideally, captured BTDR-2/Boing reference
   audio for calibration.

Until stage 7, the model should remain labelled a Reference Draft rather than a
component-accurate clone.

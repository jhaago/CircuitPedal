# Boing Reverb reference targets

Target a one-knob J. Rockett Boing-style reverb built around a TL072 analogue signal path and an Accutronics/Belton BTDR-2H reverb module.

The user-supplied Effects Layouts drawing is useful for board/component tracing, but the source labels that layout unverified. The analogue network must therefore be cross-checked before translation.

Initial cross-checked signal-path values include TL072 dual op-amp stages, BTDR-2H, A10k REVERB control, 33k/10k/22k/39k/390R/100k/15k resistors, 15nF and 4.7uF capacitors, and a 78L05 5 V supply for the brick.

BTDR-2 behavioural targets used by the first model: approximately 1.5 V peak maximum input, approximately -3 dB nominal output per channel, and short/medium/long T60 targets of roughly 2.0/2.5/2.85 seconds. The exact brick suffix in the original/reference Boing still needs confirmation, so the first reference processor defaults to medium decay.

Until physical listening and ideally hardware capture/calibration are complete, this remains a Reference Draft rather than an exact clone.

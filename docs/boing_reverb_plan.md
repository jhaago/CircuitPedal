# Boing Reverb implementation plan

Target: J. Rockett-style one-knob Boing reverb using TL072 analogue stages around a BTDR-2H Belton reverb module.

Implementation principles:
- keep the analogue input/mix/output circuitry in the generic circuit solver;
- model BTDR-2H as a reusable black-box component rather than replacing the whole pedal with a generic reverb effect;
- keep processing at the current 1x live baseline;
- allocate delay memory at compile/reset time, never in the realtime sample path;
- add decay, gain, impulse-response and stability tests before physical listening.

Current stage: reusable BTDR-2 behavioural component plus a one-knob Boing reference processor and automated tests. Next: add a TL072 device alias, expose BTDR-2 to the circuit-file architecture, then translate the surrounding analogue Boing network.

The first BTDR-2 model is a compact behavioural approximation and is not claimed to reproduce a specific physical brick until calibrated against measured hardware.

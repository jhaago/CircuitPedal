# Boing Reverb implementation plan

This document is a temporary design note for the first CircuitPedal time-based effect.

Target: J. Rockett-style one-knob Boing reverb using TL072 analogue stages around a BTDR-2H Belton reverb module.

Implementation principles:
- keep the analogue input/mix/output circuitry in the generic circuit solver;
- model BTDR-2H as a reusable black-box component rather than replacing the whole pedal with a generic reverb effect;
- keep processing at the current 1x live baseline;
- allocate delay memory at compile/reset time, never in the realtime sample path;
- add decay, gain, impulse-response and stability tests before physical listening.

The first BTDR-2 model is a compact behavioural approximation and is not claimed to reproduce a specific physical brick until calibrated against measured hardware.

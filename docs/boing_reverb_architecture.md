# Boing hybrid architecture

The intended production model is hybrid by design: analogue R/C/op-amp sections remain circuit-solved, while the real pedal's BTDR-2 digital module is represented by a reusable black-box behavioural component with explicit electrical pin scaling. This preserves CircuitPedal's circuit-first philosophy without pretending a potted digital module is an analogue subcircuit.

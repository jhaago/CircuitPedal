# Woolly Mammoth DC model triage — 2026-09-11

Condition: **9.33 V supply, no audio input, all controls at maximum.**

This report compares the published working-board voltage set against both CircuitPedal's compact 2N3904 model and the existing fuller ngspice Gummel-Poon Q2N3904 candidate.

The ngspice model remains diagnostic only because its exact manufacturer/library provenance has not yet been established in this repository.

| Node | Published board (V) | CircuitPedal compact 2N3904 (V) | CP error | ngspice Gummel-Poon (V) | ngspice error |
| --- | ---: | ---: | ---: | ---: | ---: |
| B1 | 0.5800 | 0.6165 | 6.29% | 0.5970 | 2.93% |
| C1_NODE | 1.2000 | 1.4771 | 23.09% | 1.7201 | 43.34% |
| E2 | 0.8800 | 0.8371 | 4.88% | 1.0697 | 21.56% |
| C2_NODE | 2.3000 | 1.7312 | 24.73% | 1.1301 | 50.87% |

Mean absolute percentage error:

- CircuitPedal compact model: **14.75%**
- ngspice candidate: **29.67%**

## Interpretation

The fuller candidate transistor model does **not** move the overall DC operating point closer to the published working board; it makes the aggregate disagreement materially larger.

Therefore:

- do not replace CircuitPedal's compact 2N3904 model merely because the ngspice model is more detailed;
- do not tune Woolly resistor/component values solely to fit the published voltage table;
- re-check physical-reference conditions, device spread and reference provenance before changing sound-critical model parameters;
- use a nominated physical pedal/verified clone and/or manufacturer-provenance transistor model as the next stronger reference.

This result supports keeping the current Woolly topology/model unchanged while the project continues physical listening and gathers better reference evidence.

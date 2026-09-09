# CircuitPedal component and model coverage

This file tracks what the generic `.cpedal` engine can represent and which
real pedal circuits have reached each validation stage.

## Component support in V0.9

| Component/device | Status | Current model scope |
| --- | --- | --- |
| Resistor | Working | Linear |
| Capacitor | Working | Backward-Euler companion |
| DC/audio voltage source | Working | Ideal |
| Potentiometer | Working | Linear, audio/log, explicit exponent |
| Diode | Working | Exponential PN-junction |
| NPN BJT | Working | Compact Ebers-Moll |
| PNP BJT | Working | Compact polarity-reversed Ebers-Moll |
| N-channel JFET | Working | Compact depletion square-law + gate junctions |
| N-channel MOSFET | Working | Compact enhancement square-law + body diode |
| Op-amp | Working, early | Static high-gain nonlinear controlled source with rail limiting |
| SPST/SPDT switch | Not yet | Planned |
| Linked / dual-gang pot | Not yet | Planned |
| P-channel JFET/MOSFET | Not yet | Add when a reference circuit needs it |
| Inductor / transformer | Not yet | Later |
| Full SPICE/Gummel-Poon semiconductor model | Not yet | Fidelity phase |

The word **Working** means the component has automated numerical validation and
can be used by the generic MNA engine. It does not mean every named physical
part is already a manufacturer-accurate model.

## Named model aliases in V0.9

NPN BJTs currently include compact aliases for:

- 2N3904
- 2N2222 / 2N2222A
- 2N5088
- 2N5133
- BC239 / BC239C
- BC550 / BC550C
- KT3102 / KT3102E

PNP BJTs include:

- generic PNP
- AC128 / generic germanium PNP
- 2N1308

N-channel JFETs include:

- 2N5457
- J201
- J113
- MPF4393
- 2N5952

N-channel MOSFETs include:

- BS170

Op-amps include early compact aliases for:

- JRC4558 / 4558
- CA3130 / CA3130E / CA3130EZ

Diodes include:

- 1N4148 / 1N914 / KD521
- 1N34A / germanium
- 1N6263 / Schottky

## Real circuit model status

### Live-tested

- Woolly Mammoth Reference Draft — user has played the V0.7 generic model live
  and reported plausible fuzz behaviour and useful response from all four controls.

### Automated end-to-end validation

- Big Muff — Triangle
- Big Muff — Ram's Head
- Big Muff — Green Russian
- Big Muff — NYC
- Naga Viper / Hydra Treble Booster
- Woolly Mammoth Reference Draft
- generic two-transistor fuzz demo

These files are parsed from disk, compiled through the generic circuit engine and
run through nonlinear audio in CI. This validates the architecture and numerical
stability, not exact audible matching to an original commercial pedal.

### Supplied references queued for conversion

The supplied colour-coded layouts are being used as the model backlog:

- Blueberry Bass Overdrive — CA3130 + 2N5457; engine devices now exist.
- Lovepedal Kalamazoo — 4558 + diodes; engine devices now exist.
- TS10 Tube Screamer — 4558 + NPN buffers + diodes; engine devices now exist.
- ST9 Super Tube Screamer — 4558 + NPN + diodes; needs linked/dual-gang Mids control.
- Arctic White Fuzz — BC550C + J113; engine devices now exist.
- Baby Blue OD — multiple N-JFET stages; engine devices now exist.
- Honey Bee — CA3130 + JFET stages + LEDs; engine devices now exist.
- Pink Purple Fuzz — JFET + NPN + germanium PNP; engine devices now exist.
- Fuzz Factory — NPN + germanium PNP; needs five-control GUI before convenient live use.
- Fat Fuzz Factory — same plus switchable capacitor network; needs switch support.
- Fuzzolo — NPN + BS170; engine devices now exist, plus input-mode switch if both modes are exposed.
- Galileo Mk II — multiple MPF4393 stages plus NPN; engine devices now exist.
- Dirty Little Secret Mk III — large multi-JFET topology and supply section; engine devices now exist for the audio stages, but it is intentionally later in the queue.

## Conversion rule

A new pedal should only move into the validated list after:

1. the layout/schematic has been reconstructed into an explicit node netlist;
2. a second representation is used where practical to cross-check ambiguous nodes;
3. the `.cpedal` file parses and completes a DC operating-point solve;
4. a finite input signal runs through the 4x generic processor without failed
   nonlinear timesteps;
5. control directions are checked against the physical control semantics;
6. hardware/SPICE comparison is added when a fidelity claim is desired.

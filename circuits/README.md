# CircuitPedal circuit files

V0.7 introduces the first human-readable `.cpedal` circuit format.

- `two_transistor_fuzz_demo.cpedal` is a deliberately generic two-stage fuzz
  used to exercise file loading and live controls.
- `woolly_mammoth_reference_draft.cpedal` is the first real-world target. Its
  topology is reconstructed from the commonly published two-2N3904 Woolly
  Mammoth schematic and the stripboard layout supplied during development.

The Woolly file is deliberately labelled **Reference Draft**. CircuitPedal's
current 2N3904 is a compact Ebers-Moll approximation, and the circuit has not yet
been fitted against measured hardware or a manufacturer SPICE model. The file is
therefore suitable for architecture and listening tests, not a claim of exact
commercial-pedal reproduction.

## V0.7 syntax

```text
CPEDAL 1
NAME "Example"

V VCCSRC VCC 0 9
AUDIO GUITAR INPUT 0 0.2

R R1 VCC N1 51k
C C1 INPUT B1 220n
Q Q1 C B E 2N3904
D D1 CLIP 0 1N4148
POT GAIN 0 WIPER SIGNAL 100k LIN 0.5

OUTPUT OUT 1
```

Engineering notation such as `4k99`, `2k2`, `220n`, `100u` and `1M`
is accepted. Comments may start with `#` or `//`.

Built-in semiconductor names in V0.7 are:

- NPN: `2N3904`, `GENERIC_NPN`
- diode: `1N4148`/`SILICON`, `1N34A`/`GERMANIUM`, `GENERIC_DIODE`

Potentiometers become named live controls in the macOS GUI. The V0.7 GUI shows
the first four controls; the engine supports up to sixteen.

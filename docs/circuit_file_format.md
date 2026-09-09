# CircuitPedal .cpedal format — V0.7

The V0.7 circuit file is intentionally small and human-readable. It is designed
so a reviewed schematic can be translated into a circuit definition without
writing or recompiling a C++ effect algorithm.

## Required structure

Every file begins with:

```text
CPEDAL 1
```

and must contain an `OUTPUT` directive.

Nodes are created when first referenced. `0`, `GND` and `GROUND` all mean
the global ground node.

## Directives

```text
NAME "Display Name"

V <id> <positive> <negative> <dc-volts>
AUDIO <id> <positive> <negative> <volts-per-full-scale>

R <id> <node-a> <node-b> <resistance>
C <id> <node-a> <node-b> <capacitance>

D <id> <anode> <cathode> <model>
Q <id> <collector> <base> <emitter> <model>

POT <name> <terminal1> <wiper> <terminal3> <resistance> <taper> <initial>

OUTPUT <node> [digital-full-scale-per-volt]
```

The component `id` fields are currently descriptive and do not affect the
equations. POT names become live controls in the GUI.

## Engineering notation

The parser accepts common pedal notation including:

- `4k99` = 4.99 kOhm
- `2k2` = 2.2 kOhm
- `1M` = 1 megohm
- `220n` = 220 nF
- `100u` = 100 uF
- `6.8p` = 6.8 pF

Upper-case `M` means mega; lower-case `m` means milli.

## Potentiometers

V0.7 supports:

- `LIN` / `LINEAR`
- `LOG` / `AUDIO`
- `EXP:x` for an explicit taper exponent

The initial value is normalized from 0 to 1.

A pot can also be wired as a rheostat by tying the wiper node to one end in the
file, just as the physical pot would be wired.

## Built-in semiconductor models

NPN:

- `2N3904`
- `GENERIC_NPN`

Diode:

- `1N4148` / `SILICON`
- `1N34A` / `GERMANIUM`
- `GENERIC_DIODE`

These V0.7 semiconductor models are compact solver models. The named 2N3904 is
not yet a complete manufacturer Gummel-Poon model, so CircuitPedal does not claim
component-accurate reproduction from the name alone.

## macOS loading workflow

1. Stop audio.
2. Click **Load .cpedal…**.
3. Choose a circuit file.
4. CircuitPedal parses and validates it.
5. The first four named POT controls appear in the V0.7 GUI.
6. Click **Start Audio**.
7. CircuitPedal calculates the DC operating point and starts the real-time MNA
   transient solver.
8. Pot controls can then be moved live.

Loading, topology compilation and DC setup are deliberately outside the audio
callback. Live pot targets are atomic and the transient solver uses preallocated
working storage.

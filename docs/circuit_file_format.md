# CircuitPedal .cpedal format — V0.10

The circuit file is intentionally small and human-readable. It is designed
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
PNP <id> <collector> <base> <emitter> <model>
JFET <id> <drain> <gate> <source> <model>
NMOS <id> <drain> <gate> <source> <model>
OPAMP <id> <plus> <minus> <out> <positive-rail> <negative-rail> <model>

POT <name> <terminal1> <wiper> <terminal3> <resistance> <taper> <initial>
POT_LINK <existing-control-name> <terminal1> <wiper> <terminal3> <resistance> <taper>

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

Potentiometers support:

- `LIN` / `LINEAR`
- `LOG` / `AUDIO`
- `EXP:x` for an explicit taper exponent

The initial value is normalized from 0 to 1.

A pot can also be wired as a rheostat by tying the wiper node to one end in the
file, just as the physical pot would be wired.

For dual-gang or otherwise mechanically linked controls, define the first gang
with `POT`, then add the other electrical section(s) with `POT_LINK` using the
same control name. The linked section inherits the primary control's initial
position and moves from the same live GUI control. This is intended for circuits
such as the ST9 Super Tube Screamer's dual-gang Mids control.

## Built-in device models

V0.9 adds compact named aliases for common pedal parts.

- NPN: `2N3904`, `2N2222A`, `2N5088`, `2N5133`, `BC239C`,
  `BC550C`, `KT3102E`
- PNP: `AC128`, `2N1308`, `GENERIC_PNP`
- N-JFET: `2N5457`, `J201`, `J113`, `MPF4393`, `2N5952`
- N-MOSFET: `BS170`
- op-amp: `JRC4558` / `4558`, `CA3130`
- diode: `1N4148`, `1N914`, `KD521`, `1N34A`, `1N6263`

These are compact real-time solver models. They are deliberately not described
as complete manufacturer SPICE models. The op-amp model in particular is still
an early static high-gain/rail-limited device; GBW, slew-rate, output-current and
input-bias refinements remain fidelity work.

## macOS loading workflow

1. Stop audio.
2. Click **Load .cpedal…**.
3. Choose a circuit file.
4. CircuitPedal parses and validates it.
5. Up to sixteen named controls appear in the scrollable macOS GUI.
6. Click **Start Audio**.
7. CircuitPedal calculates the DC operating point and starts the real-time MNA
   transient solver.
8. Pot controls can then be moved live.

Loading, topology compilation and DC setup are deliberately outside the audio
callback. Live pot targets are atomic and the transient solver uses preallocated
working storage.

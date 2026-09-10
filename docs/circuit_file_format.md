# CircuitPedal .cpedal format — V0.17

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

SWITCH <name> SPST <a> <b> <initial> [<off-label> <on-label>]
SWITCH <name> SPDT <common> <throw-a> <throw-b> <initial> [<a-label> <b-label>]
SWITCH <name> ONOFFON <common> <throw-a> <throw-b> <initial> [<a-label> <off-label> <b-label>]
SWITCH_LINK <existing-switch-name> <a> <b>
SWITCH_LINK <existing-switch-name> <common> <throw-a> <throw-b>

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

## Switches

V0.11 supports three live switch forms:

- `SPST`: position 0 / `OFF` is open; position 1 / `ON` is closed.
- `SPDT`: position 0 / `A` selects throw A; position 1 / `B` selects throw B.
- `ONOFFON`: position 0 / `A` selects throw A, position 1 / `OFF` or `CENTER`
  disconnects both throws, and position 2 / `B` selects throw B.

Optional quoted labels become the choices shown in the macOS control panel. The
switch remains in the MNA topology at every position: closed contacts use a very
low resistance and open contacts use a finite very-high resistance. That avoids
rebuilding or allocating a new circuit topology in the real-time callback.

For DPDT and other mechanically linked multi-pole switches, define the first
electrical pole with `SWITCH` and add the remaining pole(s) with `SWITCH_LINK`.
The linked pole inherits the primary switch mode and initial position and follows
the same GUI control. SPST links use two nodes; SPDT and ONOFFON links use a
common node plus two throws. This is the switch equivalent of `POT_LINK` and is
intended for circuits such as the Human Gear Animato's DPDT Bias switch.

## Built-in device models

The current format includes compact named aliases for common pedal parts.

- NPN: `NTE103` (germanium), `2N3904`, `2SC1815`, `2SC2240` / `C2240`,
  `2N2222A`, `2N5088`, `2N5089`, `2N5133`, `BC239C`, `BC550C`, `KT3102E`
- PNP: `NTE102` (germanium), `AC128`, `2N1308`, `2N3906`, `GENERIC_PNP`
- N-JFET: `2N5457`, `J201`, `J113`, `MPF4393`, `2N5952`
- N-MOSFET: `BS170`
- op-amp: `4558`, `4558D`, `JRC4558`, `JRC4558D`, `CA3130`, `CA3130E`, `CA3130EZ`
- diode: `1N4148`, `1N914`, `1S1588`, `KD521`, `1N4001`, `1N4007`,
  `LED_RED`, `1N34A`, `1N6263`

Common descriptive aliases such as `SILICON`, `RECTIFIER`, `LED` / `RED_LED`,
`GERMANIUM` and `SCHOTTKY` are also accepted by the compact diode-model lookup.

These are compact real-time solver models. They are deliberately not described
as complete manufacturer SPICE models. The op-amp aliases include finite
gain-bandwidth and slew-rate behaviour; output-current, detailed input-stage
behaviour and manufacturer macromodel fitting remain fidelity work.

The generic op-amp primitive currently exposes only the signal pins and supply
rails. Device-specific auxiliary pins such as the CA3130 external compensation
pins are not yet represented explicitly. Circuits such as the V0.17 Blueberry
reference draft therefore absorb that behaviour into the compact device model
until a higher-fidelity primitive is introduced.

## macOS loading workflow

1. Stop audio.
2. Click **Load External…** or choose a bundled circuit from **Choose Circuit…**.
3. Choose a circuit file if loading externally.
4. CircuitPedal parses and validates it.
5. Up to sixteen named controls appear in the scrollable macOS GUI.
6. Click **Start Audio**.
7. CircuitPedal calculates the DC operating point and starts the real-time MNA
   transient solver.
8. Pot and switch controls can then be moved live.

Loading, topology compilation and DC setup are deliberately outside the audio
callback. Live control targets are atomic and the transient solver uses
preallocated working storage.

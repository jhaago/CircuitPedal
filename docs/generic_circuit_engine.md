# CircuitPedal V0.6 generic circuit engine

V0.6 introduces the first topology-independent analogue circuit solver in CircuitPedal.

The existing Distortion+ live model remains intact and is still the default macOS
test path. The new engine is being validated alongside it before the live GUI is
switched to circuit-file loading.

## What V0.6 can represent

A `CircuitDefinition` can currently contain:

- named electrical nodes plus ground;
- resistors;
- capacitors using backward-Euler companion models;
- independent DC voltage sources;
- an audio-driven voltage source for the guitar input;
- exponential diode junctions;
- NPN bipolar transistors using a compact Ebers-Moll model;
- three-terminal potentiometers with linear or tapered position;
- a selected output node and digital output calibration.

The topology is data. The solver does not contain a dedicated signal-processing
algorithm for the circuit being described.

## Solver architecture

The circuit is compiled on a non-real-time thread into a dense Modified Nodal
Analysis system. Compilation may allocate memory and performs a DC operating-point
solve.

The transient path:

1. begins from the last converged solution;
2. stamps resistors, potentiometers, voltage sources and capacitor companions;
3. stamps nonlinear diode and BJT currents plus their Jacobian derivatives;
4. solves the Newton correction with pivoted Gaussian elimination;
5. damps large voltage corrections;
6. commits capacitor history only after a converged solve;
7. restores the last good state and returns silence if a timestep fails safely.

All working vectors are preallocated during compilation. The audio-time
`processSample()` path performs no heap allocation, locking or console I/O.

## Current semiconductor scope

The V0.6 BJT is deliberately a compact Ebers-Moll model rather than a complete
SPICE Gummel-Poon implementation. It is sufficient to establish generic nonlinear
transistor topology and bias solving, but named devices such as 2N3904 still need
a component library with fitted parameters before CircuitPedal should claim
device-accurate reproduction.

The same applies to generic diode parameters.

## Validation added in V0.6

Automated tests cover:

- a DC resistor divider;
- potentiometer division and position changes;
- an RC audio transient;
- nonlinear diode clipping;
- an NPN transistor DC operating point;
- a two-transistor fuzz-like network running for one second of audio without
  non-finite values or failed transient solves.

Both the legacy Distortion+ validation and the new generic-circuit validation
must pass in CI.

## Woolly Mammoth target

The ZVEX Woolly Mammoth layout supplied during development is a useful acceptance
target because it requires two 2N3904 NPN stages, DC bias, feedback, capacitors and
four potentiometers. Publicly documented build references also describe the core
effect as a two-transistor circuit with 2N3904 devices and the same control/value
family.

V0.6 does not yet claim that the supplied stripboard image has been converted into
an exact live Woolly Mammoth model. Before doing that we will trace every electrical
node and verify the reconstructed netlist against a conventional schematic.

## V0.7 circuit files

V0.7 completes the first circuit-file milestone described here. A human-readable
`.cpedal` parser now feeds this generic solver, the macOS GUI can load those
files directly, named potentiometers become live controls, and the selected
circuit runs through the established Core Audio path.

The current workflow is:

1. inspect a schematic or verified stripboard layout;
2. reconstruct and review its electrical netlist;
3. write/export a `.cpedal` circuit file;
4. load and compile it through the generic engine;
5. solve the DC operating point at the active interface sample rate;
6. expose named potentiometers as live GUI controls;
7. run the circuit through the proven macOS real-time audio path.

The remaining work is fidelity rather than basic file-loading architecture:
generic oversampling, stronger semiconductor/device libraries, additional device
types, automated schematic-to-netlist assistance, SPICE comparison and measured
hardware validation.

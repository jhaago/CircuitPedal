# CircuitPedal V0.3

CircuitPedal is a proof-of-concept digital guitar pedal whose processing is driven by electronic-circuit equations rather than a chain of generic distortion blocks.

V0.3 adds a minimal native macOS test GUI around the V0.2 circuit and live-audio implementation. The circuit model, parameter smoothing and bypass crossfade remain the same.

## V0.3 GUI Test

This is a functional test interface, not the final CircuitPedal visual design.

On a Mac, double-click `build_and_run_gui.command`. It checks for CMake and Apple Command Line Tools, builds the project, runs the complete automated validation suite, and only launches `CircuitPedalGUI.app` if validation passes. The equivalent manual commands are:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
open build/CircuitPedalGUI.app
```

In the GUI:

1. Turn the physical interface/headphone/amplifier output down before starting.
2. Select a duplex audio interface. Only devices with both input and output capability are listed.
3. Select the physical input channel carrying the guitar; do not assume this is always input 1.
4. Start with a 64-frame buffer. If the interface does not support it reliably, stop audio and try 128 or 256.
5. Click **Start Audio**. Device, channel and buffer controls are locked until **Stop Audio** is clicked.
6. Adjust **Distortion** and **Output**, or enable **Bypass**. These controls drive the existing circuit-model parameters and bypass crossfade directly.
7. Read the input/output peak meters and the status area. The status shows sample rate, requested and actual buffer sizes, buffer duration, Core Audio input/output latency and safety offsets, DSP FIR delay, and their reported component sum.

Startup errors remain visible in the window so settings can be changed and Start can be retried. macOS may ask for microphone access on first launch; if it was denied, enable CircuitPedal under **System Settings > Privacy & Security > Microphone**.

## Circuit-engine changes introduced in V0.2

- The post-op-amp coupling capacitor, 10 kOhm clipping resistor, diode pair, 1 nF capacitor and complete output-pot load are now solved as one connected network.
- The diode solve uses safeguarded Newton iteration, consistent exponential limiting, KCL residual checks and a bounded bisection fallback.
- The input coupling and RF capacitors are solved as a connected two-node network rather than fixed cutoff filters.
- The gain capacitor has explicit companion-model state and remains stable as the virtual pot changes.
- A compact LM741-inspired model adds finite gain-bandwidth, slew rate and a rail knee without compressing the entire nominal output range.
- Real FIR interpolation and decimation filters replace linear interpolation and four-sample averaging.
- Controls are smoothed, bypass is crossfaded, and the virtual circuit continues running while bypassed.
- Non-finite inputs and parameters fail safely instead of permanently poisoning state.
- ADC/DAC scaling, source resistance and output load are explicit calibration values.
- The Core Audio app can list/select duplex devices, choose the interface input channel, request a buffer size and report latency components.
- The previous one-sine smoke test is replaced by numerical, state, sample-rate, control and long-run tests.

## Reference circuit

The exact V0.2 engineering reference and its remaining assumptions are defined in [`docs/reference_circuit.md`](docs/reference_circuit.md). That file is normative when code comments or external schematics disagree.

V0.2 still is not a general arbitrary-netlist solver. Its public audio boundary is intended to remain stable while the dedicated internals are migrated toward a compiled MNA circuit representation.

## Build and test

Requirements:

- CMake 3.16 or newer;
- a C++17 compiler;
- Apple command-line tools for the live macOS target.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On non-Apple platforms this builds and tests the circuit core only.

To run with AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
cmake -S . -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCIRCUITPEDAL_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

## macOS live audio

Start with the physical output volume low. One duplex interface is still required for both input and output.

List devices:

```bash
./build/circuitpedal --list-devices
```

Use the default output device, input channel 1 and request a 64-frame buffer:

```bash
./build/circuitpedal
```

Select another device, channel and buffer size:

```bash
./build/circuitpedal --device-id 73 --input-channel 2 --buffer-size 128
```

The program validates that the chosen device has both input and output channels. macOS may request microphone/audio-input permission for Terminal.

The terminal controls remain:

- `g` / `G`: distortion down/up;
- `o` / `O`: output down/up;
- `b`: bypass;
- `m`: recent signal peaks;
- `q`: quit.

## Latency

The app reports:

- actual hardware buffer frames;
- milliseconds per buffer period;
- device input/output latency;
- input/output safety offsets;
- the 47-sample FIR oversampling delay.

These values are not a substitute for physical loopback measurement. Converter delay and driver behaviour must be included when deciding whether a configuration feels like a hardware pedal.

## Validation status

The automated suite currently checks:

- the diode-network solution against an independent bisection reference;
- KCL residuals over source/state/timestep grids;
- silence and DC removal;
- supported sample rates;
- NaN/Inf recovery;
- extreme transients;
- deterministic output and clipping symmetry;
- harmonic growth across the distortion-control range;
- output-control monotonicity;
- delayed-dry bypass accuracy and smoothed switching;
- long random-input stability.

The remaining SPICE and physical-pedal comparison work is specified in [`docs/validation_plan.md`](docs/validation_plan.md). CircuitPedal should not claim component-accurate reproduction of a physical unit until that plan has produced passing reference data.

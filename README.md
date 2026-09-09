# CircuitPedal V0.7

CircuitPedal is a proof-of-concept digital guitar pedal whose processing is driven by electronic-circuit equations rather than a chain of generic distortion blocks.

V0.7 adds human-readable `.cpedal` circuit files and connects the generic Modified Nodal Analysis engine to the proven macOS live-audio path. A supported circuit can now be parsed, validated, DC-biased and run in real time without writing a circuit-specific C++ effect algorithm.

The built-in Distortion+ remains available as the regression/reference model. V0.7 also includes a generic two-transistor fuzz demo and a **Woolly Mammoth Reference Draft** circuit file reconstructed from the commonly published two-2N3904 topology. The Woolly file is an engineering/listening target, not yet a component-accurate clone claim.

See [`docs/circuit_file_format.md`](docs/circuit_file_format.md) for the V0.7 file format and [`docs/generic_circuit_engine.md`](docs/generic_circuit_engine.md) for the solver architecture.

## V0.7 Circuit Lab Test

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
2. Leave **Built-in Distortion+** selected for the established reference path, or click **Load .cpedal…** and choose a circuit from the `circuits/` folder.
3. Select a duplex audio interface and the physical input channel carrying the guitar.
4. Start with a 64-frame buffer. If the interface does not support it reliably, stop audio and try 128 or 256.
5. For a loaded circuit, its first four named POT controls appear automatically and can be moved live while audio is running.
6. Click **Start Audio**. A loaded circuit is compiled for the device sample rate and its DC operating point is solved before Core Audio starts.
7. **Bypass** works for both the built-in and generic circuit paths.
8. Read the meters/status area for the active model, buffer and latency information.

For the first V0.7 file-loading test, try `circuits/two_transistor_fuzz_demo.cpedal`. The next meaningful listening test is `circuits/woolly_mammoth_reference_draft.cpedal`, which exposes **PINCH, WOOL, EQ and OUTPUT**.

Startup errors remain visible in the window so settings can be changed and Start can be retried. macOS may ask for microphone access on first launch; if it was denied, enable CircuitPedal under **System Settings > Privacy & Security > Microphone**.

## Circuit-file loading introduced in V0.7

- Adds the versioned, human-readable `.cpedal` parser.
- Supports engineering notation such as `4k99`, `2k2`, `220n` and `100u`.
- Maps named POT directives to real-time-safe GUI controls.
- Adds built-in compact `2N3904`, silicon-diode and germanium-diode model names.
- Connects generic circuits to the native macOS Core Audio path.
- Compiles and solves a circuit's DC operating point before live audio starts.
- Adds a loadable two-transistor fuzz demo and Woolly Mammoth reference draft.
- Adds end-to-end parser/circuit-file validation in CI.

## Generic circuit engine introduced in V0.6

- Adds a topology-independent dense Modified Nodal Analysis solver.
- Adds named nodes, resistors, capacitors, independent/audio voltage sources and potentiometers.
- Adds exponential diode junctions and compact Ebers-Moll NPN BJT devices.
- Solves a DC operating point before transient audio processing.
- Uses Newton iteration with a stamped Jacobian and damped corrections for nonlinear devices.
- Preallocates all transient working storage so the generic `processSample()` path performs no heap allocation.
- Adds automated RC, diode, transistor and two-transistor fuzz-like validation in addition to the existing Distortion+ suite.
- Keeps the proven Distortion+ macOS live path unchanged while the generic engine matures.

## Circuit Lab changes introduced in V0.4

- Adds a typed clipping-diode preset API to the circuit core.
- Preserves the exact V0.2 reference germanium parameters as the default.
- Adds experimental silicon-like and LED-like diode curves plus a no-clipping-diode comparison mode.
- Exposes diode selection in the native macOS GUI and locks topology-affecting selection while audio is running.
- Extends automated validation across all diode presets and safe fallback behaviour.
- Keeps the real-time audio callback allocation-free and preserves the existing Distortion, Output and Bypass control path.

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

The legacy Distortion+ implementation remains a dedicated reference model. V0.7 now provides the human-readable circuit-file layer needed to load supported new pedal topologies without recompiling C++.

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
- long random-input stability;
- generic resistor-divider and potentiometer operation;
- RC transient behaviour;
- generic nonlinear diode operation;
- NPN BJT DC bias;
- one second of stable two-transistor fuzz-like transient audio.

The remaining SPICE and physical-pedal comparison work is specified in [`docs/validation_plan.md`](docs/validation_plan.md). CircuitPedal should not claim component-accurate reproduction of a physical unit until that plan has produced passing reference data.

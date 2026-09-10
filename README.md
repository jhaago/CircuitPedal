# CircuitPedal V0.12

CircuitPedal is a proof-of-concept digital guitar pedal whose processing is driven by electronic-circuit equations rather than a chain of generic distortion blocks.

V0.12 expands the built-in compatibility library with a **Fuzzolo Reference Draft**. It combines the two-transistor fuzz core with a BS170 output stage, exposes Pulse Width and Volume, and turns the production internal Passive/Active pickup jumper into a live two-position circuit switch for testing.

The V0.11 live-switch architecture, linked pots, sixteen-control GUI, bundled circuit library and 4x generic oversampling remain in place. Fuzzolo is deliberately labelled a reference draft because the current BS170 and BJT models are compact real-time approximations and the early published trace contains component-value ambiguity that should ultimately be settled against measured hardware.

See [`docs/circuit_file_format.md`](docs/circuit_file_format.md) for the circuit-file format and [`docs/generic_circuit_engine.md`](docs/generic_circuit_engine.md) for the solver architecture.

## V0.12 Circuit Lab Test

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
2. Leave **Built-in Distortion+** selected for the established reference path, or choose a bundled model from the **Choose Circuit…** menu. **Load External…** still accepts any compatible `.cpedal` file.
3. Select a duplex audio interface and the physical input channel carrying the guitar.
4. Start with a 64-frame buffer. If the interface does not support it reliably, stop audio and try 128 or 256.
5. For a loaded circuit, up to sixteen named controls appear in a scrollable panel and can be moved live while audio is running. Linked pot sections move together behind one control.
6. Click **Start Audio**. A loaded circuit is compiled for the device sample rate and its DC operating point is solved before Core Audio starts.
7. **Bypass** works for both the built-in and generic circuit paths.
8. Read the meters/status area for the active model, buffer and latency information.

The bundled library currently includes the generic two-transistor fuzz demo, Woolly Mammoth Reference Draft, Naga Viper, four Big Muff variants, Fuzz Factory Reference, Fat Fuzz Factory Reference and Fuzzolo Reference Draft. Generic circuits report the same 47-host-sample FIR delay as the reference Distortion+ oversampling path.

Startup errors remain visible in the window so settings can be changed and Start can be retried. macOS may ask for microphone access on first launch; if it was denied, enable CircuitPedal under **System Settings > Privacy & Security > Microphone**.

## Fuzzolo library expansion introduced in V0.12

- Adds a Fuzzolo reference draft using two 2N3904 stages followed by a BS170 output booster.
- Exposes Pulse Width and Volume as live circuit pots.
- Exposes the internal Passive/Active pickup-level jumper as a live SPDT control.
- Uses the later widely reported 5k1 SHO drain value rather than silently treating the ambiguous early 51k trace as authoritative.
- Adds end-to-end 4x oversampled validation across pickup-mode and Pulse Width changes.

## Live-switch changes introduced in V0.11

- Adds generic electrical SPST, SPDT and on-off-on switch devices without recompiling a circuit-specific effect.
- Uses low on-resistance and finite off-resistance so the MNA topology remains numerically well-conditioned across switch positions.
- Adds labelled discrete switch controls to the macOS circuit-control panel.
- Adds `SWITCH` syntax to `.cpedal` files, including custom labels for each switch position.
- Adds a switchable **Fat Fuzz Factory Reference** circuit with Fat / Normal / Super Fat positions.
- Extends automated validation to live switch changes, parser metadata and all three Fat Fuzz Factory positions.

## Compatibility-library changes introduced in V0.10

- Bundles all repository `.cpedal` files into the macOS app and provides an in-app circuit selector.
- Expands the macOS circuit control panel from four fixed sliders to a scrollable sixteen-control view.
- Adds `POT_LINK` so one physical/GUI control can drive multiple electrical potentiometer sections such as a dual-gang control.
- Adds a five-control Fuzz Factory reference model using separate lower/higher-gain AC128 compact models.
- Keeps external `.cpedal` loading available for development and user-created models.

## Generic device expansion introduced in V0.9

- Adds compact PNP BJT, N-channel JFET and N-channel MOSFET devices to the generic MNA solver.
- Adds an early supply-limited generic op-amp controlled-source model.
- Adds named aliases for common pedal parts including 2N2222A, 2N5088, 2N5133, BC239C, BC550C, AC128, J113, J201, 2N5457, MPF4393, 2N5952, BS170, 4558 and CA3130.
- Adds source stepping to improve DC operating-point convergence for harder nonlinear circuits.
- Adds Naga Viper and four Big Muff variant circuit files with end-to-end automated validation.

## Generic fidelity changes introduced in V0.8

- Runs supported generic circuits at 4x the host sample rate before FIR decimation.
- Uses the oversampled timestep inside the capacitor and nonlinear-device equations rather than oversampling only after the effect.
- Latency-aligns the dry path for click-free generic-circuit bypass.
- Solves the DC operating point at the exact pre-start potentiometer positions selected in the GUI.
- Corrects the Woolly Mammoth **WOOL** control direction so increasing the GUI control corresponds to increasing the physical emitter-bypass effect.
- Adds a broad automated Woolly Mammoth DC-bias sanity guard based on published voltages from a verified working build.
- Keeps the tighter physical/SPICE agreement target in the validation plan rather than overfitting the compact BJT model to one build.

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

The legacy Distortion+ implementation remains a dedicated reference model. The generic path uses the same 4x FIR oversampling concept, while the `.cpedal` layer lets supported pedal topologies load without recompiling C++.

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
- one second of stable two-transistor fuzz-like transient audio;
- 4x oversampled generic nonlinear processing;
- end-to-end Woolly Mammoth circuit-file loading;
- broad Woolly working-board DC-bias sanity checks at all controls maxed;
- PNP BJT, N-JFET, N-MOSFET and generic op-amp smoke/bias tests;
- four Big Muff circuit files end-to-end;
- Naga Viper end-to-end;
- five-control Fuzz Factory reference end-to-end;
- linked/multi-gang potentiometer parsing;
- live SPST/SPDT/on-off-on circuit switching;
- Fat Fuzz Factory across all Fat-switch positions;
- Fuzzolo across Passive/Active pickup selection and live Pulse Width changes.

The remaining SPICE and physical-pedal comparison work is specified in [`docs/validation_plan.md`](docs/validation_plan.md). CircuitPedal should not claim component-accurate reproduction of a physical unit until that plan has produced passing reference data.

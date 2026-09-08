# CircuitPedal V0.1

First runnable proof-of-concept for a physical-circuit-emulating digital guitar pedal.

## What V0.1 is

This version deliberately concentrates on the difficult part: passing live guitar audio through a **component-driven analogue circuit model** in real time.

It contains:

- a macOS Core Audio live-input/live-output application;
- a separate C++ circuit core;
- Distortion and Output controls;
- bypass;
- a numerical anti-parallel germanium diode clipping stage;
- a synthetic core test that can be built separately from the Mac audio application.

The live-audio layer uses Apple's built-in Core Audio frameworks, so V0.1 has **no third-party runtime/audio dependency**.

## What is physically modelled

`DistortionPlusModel` represents the major mechanisms of a classic MXR Distortion+-style circuit:

- 10 nF input coupling behaviour (~23.5 Hz reference corner);
- frequency-dependent op-amp feedback branch;
- 1 MOhm feedback resistor;
- 4.7 kOhm minimum gain-branch resistance;
- variable virtual gain-pot resistance;
- 47 nF gain-branch capacitor;
- finite op-amp output swing on a 9 V-style supply;
- 1 uF / 10 kOhm post-op-amp coupling behaviour;
- 10 kOhm clipping resistor;
- anti-parallel germanium diodes solved from their exponential I-V equation;
- 1 nF clipping-node capacitor;
- output-pot wiper position.

The nonlinear clipping node is not implemented as a generic `tanh()` distortion effect. Each DSP substep solves the current balance through the **virtual resistor, capacitor and two diodes** using Newton iteration.

## Important accuracy note

This is V0.1, not yet a perfect digital clone of a particular physical serial-numbered Distortion+.

The circuit topology and common component values drive the model, but some details are intentionally approximate for the first live test:

- the germanium diode parameters are approximate 1N270-like values;
- the op-amp uses a finite-output-swing approximation rather than a transistor-level LM741 model;
- input and output voltage calibration currently assume 1 digital full-scale = roughly 1 V peak;
- the input 1 nF shunt capacitor is represented by its high-frequency filtering effect rather than a complete pickup/source-impedance model;
- 4x nonlinear substepping is present, but proper anti-alias oversampling filters are a V0.2 task.

These are all replaceable without changing the Mac audio application.

## Build on Mac

### 1. Install Apple's command-line tools

In Terminal:

```bash
xcode-select --install
```

### 2. Install CMake

If you already use Homebrew:

```bash
brew install cmake
```

### 3. Build

```bash
cd CircuitPedal_v0_1
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 4. Test the circuit core

```bash
./build/circuit_core_test
```

You should see `PASS` plus input/output measurements.

### 5. Prepare the audio interface

For this first version, use **one USB audio interface for both input and output**.

1. Guitar -> interface Instrument / Hi-Z input.
2. Headphones, powered monitors, or a suitable re-amp/output path from the same interface.
3. In macOS Sound settings, make that interface the default **Output** device.
4. Start with physical output volume LOW.

V0.1 attaches its input side to that same Core Audio device. A device picker and independent input/output device support come later.

### 6. Run the pedal

```bash
./build/circuitpedal
```

macOS may request microphone/audio-input permission for Terminal. Allow it.

## Controls

Type the character and press Enter:

- `g` = Distortion down
- `G` = Distortion up
- `o` = Output down
- `O` = Output up
- `b` = bypass toggle
- `m` = show recent input/output peaks
- `q` = quit

The controls are crude on purpose. The next UI will use knobs.

## Project structure

```text
CircuitPedal_v0_1/
├── CMakeLists.txt
├── README.md
└── src/
    ├── DistortionPlusModel.h
    ├── DistortionPlusModel.cpp
    ├── core_test.cpp
    └── main_mac.cpp
```

The important architectural separation is:

```text
Core Audio / future hardware ADC
             |
             v
     DistortionPlusModel
             |
             v
Core Audio / future hardware DAC
```

That means the circuit engine can later be moved into physical pedal hardware without taking the Mac application with it.

## What I recommend for V0.2

1. Proper Mac GUI with two rotary knobs, bypass and meters.
2. Audio interface/device selection.
3. Actual measured round-trip latency display.
4. Proper 4x or 8x anti-alias oversampling.
5. Input-voltage calibration.
6. Diode selector: 1N270 / 1N34A / 1N4148 / LED.
7. Editable component values.
8. Offline WAV/SPICE comparison harness.
9. Start the generic circuit graph/netlist engine.

The major milestone after that is replacing the dedicated Distortion+ class with something that can consume a schematic-derived netlist.

# CircuitPedal HTML UI Sandbox

This folder is an isolated visual/UX prototype for CircuitPedal. It does not run the C++ DSP engine and must not be treated as an audio-validation build.

## Purpose

Use the sandbox to iterate quickly on the visual language and interaction model for:

- the future CircuitPedal desktop UI;
- the eventual phone/tablet companion app;
- pedal/package presentation and control layouts;
- signal-chain and live-performance workflows.

## Current prototype

`index.html` is fully self-contained and can be opened directly in a browser. It includes:

- dark premium application shell;
- pedal library and search;
- selectable example pedals;
- signal-chain strip;
- large pedal hero view;
- touch/drag rotary controls;
- parameter inspector;
- bypass/active state;
- simulated input/output meters;
- simulated live monitor;
- scenes and placeholder quick actions;
- fullscreen control;
- landscape/tablet-oriented layout.

The UI deliberately labels future-only features rather than pretending they are implemented in the real application.

## Android / PWA path

`launch.html`, `manifest.webmanifest`, `sw.js` and `circuitpedal-ui.svg` make the sandbox PWA-ready when the folder is served over HTTPS (for example through a future GitHub Pages deployment).

Once hosted, Android Chrome can install it to the home screen and run it fullscreen in landscape orientation.

## Important boundary

This prototype is visual only. It does not change or validate:

- the 48 kHz / 1x DSP path;
- circuit models;
- pedal fidelity;
- live audio latency;
- package loading in the native app.

Approved visual ideas should be ported deliberately into the native macOS UI and eventual companion app rather than merging browser code into the audio engine.

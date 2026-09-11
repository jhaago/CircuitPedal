# CircuitPedal HTML UI Sandbox

This folder is an isolated visual/UX prototype for CircuitPedal. It does not run the C++ DSP engine and must not be treated as an audio-validation build.

## Purpose

Use the sandbox to iterate quickly on the visual language and interaction model for:

- the future CircuitPedal desktop UI;
- the eventual phone/tablet companion app;
- pedal/package presentation and control layouts;
- signal-chain and live-performance workflows.

## Current direction

`launch.html` now opens `balanced.html` by default.

The balanced prototype is intentionally between the two earlier extremes:

- the pedal remains the dominant visual element;
- the left pedal/preset browser is narrow and secondary;
- the right side contains one contextual panel with Circuit / Analysis / Info tabs rather than permanent technical panels;
- the signal chain is a separate view opened only when needed;
- the bottom strip is kept thin and limited to audio I/O, level meters, bypass and latency;
- the main editable controls remain directly on the pedal.

Earlier visual experiments are retained for comparison:

- `index.html` — original dashboard-heavy pass;
- `focus.html` — stripped-back pedal-dominant pass;
- `match.html` — richer workstation/reference-matched pass;
- `balanced.html` — current preferred middle-ground direction.

## Android / PWA path

`launch.html`, `manifest.webmanifest`, `sw.js` and `circuitpedal-ui.svg` make the sandbox PWA-ready when the folder is served over HTTPS.

Once hosted, Android Chrome can install it to the home screen and run it fullscreen in landscape orientation.

## Important boundary

This prototype is visual only. It does not change or validate:

- the 48 kHz / 1x DSP path;
- circuit models;
- pedal fidelity;
- live audio latency;
- package loading in the native app.

Approved visual ideas should be ported deliberately into the native macOS UI and eventual companion app rather than merging browser code into the audio engine.

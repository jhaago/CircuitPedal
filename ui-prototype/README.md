# CircuitPedal HTML UI Sandbox

This folder is an isolated visual/UX prototype for CircuitPedal. It does not run the C++ DSP engine and must not be treated as an audio-validation build.

## Canonical direction

`app.html` is now the canonical CircuitPedal UI prototype. `launch.html` registers the offline service worker and redirects directly to `app.html` rather than embedding the UI in an iframe.

The locked layout direction is:

- compact application/status header;
- narrow Presets / Pedals / Favorites browser on the left;
- compact physical-pedal-style signal chain above the hero area;
- large central selected-pedal hero with controls directly on the enclosure;
- permanent right inspector with Parameters / Circuit / EQ / Settings tabs;
- compact bottom live-performance strip with meters, monitor, scenes, bypass and save controls.

The selected pedal remains the dominant visual object, while the surrounding UI provides context and deeper editing without turning the screen into a generic engineering dashboard.

## Implemented prototype interactions

The canonical prototype currently simulates:

- preset selection, next/previous navigation and favorites;
- Presets / Pedals / Favorites browser tabs and search;
- signal-chain pedal selection;
- hero pedal changes based on the selected chain effect;
- touch/mouse rotary controls;
- synchronization between hero-pedal and inspector knob values;
- Parameters / Circuit / EQ / Settings inspector tabs;
- scene A/B/C/D selection;
- bypass from both the pedal footswitch and global performance control;
- animated input/output meters and live monitor;
- device selectors, audio-active status and simulated performance information;
- browser fullscreen plus an internal immersive fallback.

All audio, circuit and analysis values are visual prototype data only.

## Historical comparison prototypes

These older experiments remain available for comparison but should not be evolved in parallel with the canonical page:

- `index.html` — original dashboard-heavy pass;
- `focus.html` — stripped-back pedal-dominant pass;
- `match.html` — richer workstation/reference-matched pass;
- `balanced.html` — middle-ground exploration;
- `app.html` — canonical locked-layout direction.

## Android / PWA path

`launch.html`, `manifest.webmanifest`, `sw.js` and `circuitpedal-ui.svg` keep the sandbox PWA-ready when the folder is served over HTTPS.

Once hosted, Android Chrome can install it to the home screen and run it fullscreen in landscape orientation.

## Important boundary

This prototype does not change or validate:

- the 48 kHz / 1x DSP path;
- circuit models;
- pedal fidelity;
- live audio latency;
- package loading in the native app;
- native macOS GUI behaviour.

Approved visual ideas should be ported deliberately into the native UI and eventual companion app rather than merging browser code into the audio engine.

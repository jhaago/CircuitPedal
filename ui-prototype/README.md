# CircuitPedal HTML UI Sandbox

This folder is an isolated visual/UX prototype for CircuitPedal. It does not run the C++ DSP engine and must not be treated as an audio-validation build.

## Canonical direction

`app.html` is the canonical CircuitPedal UI prototype. `launch.html` registers the offline service worker and redirects directly to `app.html` rather than embedding the UI in an iframe.

The current locked layout direction is:

- compact application/status header;
- narrow Presets / Pedals / Favorites browser on the left;
- editable physical-pedal-style signal chain above the hero area;
- large central selected-pedal hero with controls directly on the enclosure and an atmospheric pedal-specific background;
- permanent right inspector with Parameters / Circuit / EQ / Settings tabs;
- minimal bottom strip with input/output metering, signal-path summary and global bypass.

The selected pedal remains the dominant visual object, while deeper editing stays available without permanently filling the screen with analysis panels.

## Signal-chain interaction prototype

The chain is now treated as a future routing workspace rather than a static visual strip. The browser prototype currently demonstrates:

- adding pedals from the Pedals library;
- removing pedals from the chain;
- touch/mouse drag reordering;
- series routing;
- a simple Parallel A / B routing mode;
- moving pedals between parallel path A and B;
- selecting a chain pedal to edit it in the hero and inspector.

This is only a UX simulation. No multi-effect DSP routing has been implemented in the native engine yet.

## Implemented prototype interactions

The canonical prototype currently simulates:

- preset selection, next/previous navigation and favorites;
- Presets / Pedals / Favorites browser tabs and search;
- signal-chain editing and routing as described above;
- hero pedal changes based on the selected effect;
- touch/mouse rotary controls;
- synchronization between hero-pedal and inspector knob values;
- Parameters / Circuit / EQ / Settings inspector tabs;
- bypass from both the pedal footswitch and global bypass control;
- animated input/output meters;
- device selectors, audio-active status and simulated performance information;
- Save / Save As and related preset actions inside the burger menu;
- browser fullscreen plus an internal immersive fallback.

The previous bottom scene buttons and live-monitor graph were deliberately removed after tablet review because they competed with the main pedal without adding enough value.

All audio, circuit and analysis values are visual prototype data only.

## Historical comparison prototypes

These older experiments remain available for comparison but should not be evolved in parallel with the canonical page:

- `index.html` — original dashboard-heavy pass;
- `focus.html` — stripped-back pedal-dominant pass;
- `match.html` — richer workstation/reference-matched pass;
- `balanced.html` — middle-ground exploration;
- `app.html` — canonical direction.

## Android / PWA path

`launch.html`, `manifest.webmanifest`, `sw.js` and `circuitpedal-ui.svg` keep the sandbox PWA-ready when the folder is served over HTTPS.

Once hosted, Android Chrome can install it to the home screen and run it fullscreen in landscape orientation.

## Important boundary

This prototype does not change or validate:

- the 48 kHz / 1x DSP path;
- circuit models;
- pedal fidelity;
- live audio latency;
- real multi-effect serial/parallel routing;
- package loading in the native app;
- native macOS GUI behaviour.

Approved visual ideas should be ported deliberately into the native UI and eventual companion app rather than merging browser code into the audio engine.

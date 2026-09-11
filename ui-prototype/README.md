# CircuitPedal HTML UI Sandbox

This folder is an isolated visual/UX prototype for CircuitPedal. It does not run the C++ DSP engine and must not be treated as an audio-validation build.

## Canonical direction

`app.html` is the canonical CircuitPedal UI prototype. `launch.html` registers the offline service worker and redirects directly to `app.html` rather than embedding the UI in an iframe.

The current locked layout direction is:

- compact application/status header;
- narrow Presets / Pedals / Favorites browser on the left;
- compact signal-chain overview above the hero area;
- large central selected-pedal hero with controls directly on the enclosure and an atmospheric pedal-specific background;
- permanent right inspector with Parameters / Circuit / EQ / Settings tabs;
- minimal bottom strip with master input/output controls, level meters, signal-path summary and global bypass.

The selected pedal remains the dominant visual object during normal use. Deeper signal-routing work is moved into a dedicated editor rather than permanently occupying the main pedal view.

## Signal-chain editor prototype

The compact chain strip is now an overview. `Edit Signal Chain` transforms the whole centre workspace into a freeform routing canvas while keeping the pedal library on the left and the contextual inspector on the right.

The browser prototype currently demonstrates:

- placing/removing pedals from the Pedals library;
- freely dragging pedal nodes around the canvas;
- drawing routing cables by tapping an output port and then an input port;
- splitting one output to multiple downstream pedals;
- recombining multiple paths into a downstream stage;
- deleting individual cables by tapping them;
- selecting a routing node to edit that pedal in the right inspector;
- resetting the graph to a simple serial path;
- clearing all cables without removing the pedals.

The initial example graph intentionally demonstrates a split/recombine topology so parallel routing is visible without a separate Parallel mode.

This is only a UX simulation. No multi-effect routing graph has been implemented in the native DSP engine yet.

## Master I/O controls

The bottom strip now includes:

- `Input Trim` beside the input meter, simulated from -18 dB to +18 dB;
- `Output Level` beside the output meter, simulated from -18 dB to +6 dB;
- global bypass;
- current pedal/preset context;
- a compact routing summary.

The master controls currently affect only the simulated browser metering. They do not control the physical audio-interface preamp or the native audio engine.

## Implemented prototype interactions

The canonical prototype currently simulates:

- preset selection, next/previous navigation and favorites;
- Presets / Pedals / Favorites browser tabs and search;
- compact signal-chain overview plus the freeform routing editor described above;
- hero pedal changes based on the selected effect;
- touch/mouse rotary controls;
- synchronization between hero-pedal and inspector knob values;
- Parameters / Circuit / EQ / Settings inspector tabs;
- bypass from both the pedal footswitch and global bypass control;
- master Input Trim and Output Level controls;
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
- real multi-effect serial/parallel/freeform routing;
- package loading in the native app;
- native macOS GUI behaviour.

Approved visual ideas should be ported deliberately into the native UI and eventual companion app rather than merging browser code into the audio engine.

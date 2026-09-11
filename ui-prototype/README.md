# CircuitPedal UI prototype

This folder is an isolated visual/UX sandbox. It does not process audio and it does not change the C++ DSP engine.

## Current default

`launch.html` now opens `match.html`, the screenshot-inspired premium direction selected during tablet review.

The target is a pedal-centred professional audio application rather than a stripped-down single-pedal screen. The main composition is:

- compact top navigation;
- left device/chain and preset browser;
- circuit/schematic panel;
- large central pedal hero with touch-adjustable controls;
- right frequency/time analysis stack;
- compact bottom audio I/O, meters, bypass and latency strip.

The visual reference is treated as a composition/polish target rather than a pixel-for-pixel copy. CircuitPedal branding, labels, circuit graphics and pedal styling remain original.

## Prototype variants

- `match.html` — current primary direction, closest to the premium multi-panel reference.
- `focus.html` — earlier pedal-dominant minimalist experiment retained for comparison.
- `index.html` — original dashboard-style experiment retained for comparison.
- `launch.html` — wrapper used for PWA/fullscreen testing; currently opens `match.html`.

## Tablet interaction

- Drag vertically on pedal knobs to adjust them.
- Mouse-wheel knob adjustment also works on desktop.
- Presets are selectable.
- Bypass toggles the pedal LED state.
- Meters are simulated for visual testing.
- `FULLSCREEN` attempts the browser Fullscreen API and also enters a CSS immersive mode, so there is a visible response even where the browser blocks true fullscreen.

## PWA notes

`manifest.webmanifest`, `sw.js` and `circuitpedal-ui.svg` are included so this can later be installed as an Android home-screen app once hosted over HTTPS. GitHub Pages must first be enabled for the repository before automated Pages deployment can be added.

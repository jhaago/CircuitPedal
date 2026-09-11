# Pedal package implementation status

## Current foundation

The package foundation is intentionally separate from the Circuit Lab visual redesign and is based on the current stable `main` processing baseline.

Implemented:

- cross-platform `pedal.json` parser and schema validation;
- normalized UI control-position metadata;
- package-relative circuit and preset path resolution;
- first Woolly Mammoth package manifest;
- default Woolly preset data;
- macOS app-bundle copying of the complete `pedals/` directory;
- automated discovery/validation of every `pedals/*/pedal.json` package in the repository;
- tests that every declared circuit and preset file exists;
- malformed/missing manifest and schema-error tests;
- CI verification that the packaged Woolly manifest/preset and relative circuit path resolve correctly inside the macOS `.app` bundle;
- backwards-compatible existing `.cpedal` workflow remains unchanged.

## Important current limitation

The native Circuit Lab still discovers its selectable pedal models by scanning bundled `.cpedal` files. It does **not yet use `pedal.json` as the user-facing library source**, and it does not yet expose package presets in the GUI.

Therefore the package **data/parser/bundling foundation can be tested and promoted independently**, while package-aware library browsing remains the next implementation phase.

## Deferred UI work

- package-aware pedal browser using `displayName`, category and model status;
- preset selection/application UI;
- faceplate image rendering;
- interactive controls positioned from package metadata;
- thumbnail/icon display;
- final artwork handling and generic-control fallback policy.

The older Circuit Lab visual redesign has been split into its own feature workstream so it does not block or contaminate package-core validation.

# Pedal package implementation status

## Implemented in v0.1 foundation

- cross-platform `pedal.json` parser and schema validation;
- normalized UI control positions;
- circuit-path and preset-path resolution;
- first Woolly Mammoth package manifest;
- default Woolly preset;
- macOS app-bundle copying of the complete `pedals/` directory;
- CI validation of the package parser and bundled package structure;
- backwards-compatible existing `.cpedal` workflow remains unchanged.

## Deliberately deferred until artwork approval

- faceplate image rendering in the macOS Circuit Lab;
- interactive knobs positioned over the faceplate image;
- thumbnail-based pedal browser;
- icon display;
- preset selection UI.

The UI work is deferred rather than mocked with temporary graphics. Packages with missing artwork should fall back to the generic Circuit Lab control presentation.

The `feature/pedal-packages-v0.1` branch is based on `feature/ui-aesthetic-pass`, so package integration can continue directly against the new GUI once its visual pass is physically reviewed.

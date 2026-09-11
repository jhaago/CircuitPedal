# CircuitPedal Pedal Package Format v0.1

A pedal package groups user-facing metadata, artwork references, UI control layout and presets around one authoritative `.cpedal` circuit file.

The electrical model remains separate from the package layer. Loading a package must never change the circuit solver or DSP topology by itself.

## Directory layout

```text
pedals/
  woolly_mammoth/
    pedal.json
    assets/
      faceplate.png
      thumbnail.png
      icon.png
    presets/
      default.json

circuits/
  woolly_mammoth_reference_draft.cpedal
```

During the migration period, the circuit file remains in the existing `circuits/` directory so there is only one authoritative electrical model.

## Required manifest fields

```json
{
  "id": "woolly_mammoth",
  "displayName": "Woolly Mammoth",
  "circuit": {
    "file": "../../circuits/woolly_mammoth_reference_draft.cpedal"
  }
}
```

`id` uses lowercase snake_case. `displayName` is the user-facing name. `circuit.file` is resolved relative to the directory containing `pedal.json`.

## Artwork

The `assets` object may provide:

```json
"assets": {
  "faceplate": "assets/faceplate.png",
  "thumbnail": "assets/thumbnail.png",
  "icon": "assets/icon.png"
}
```

Recommended v0.1 sizes:

- faceplate: 1600 x 1000 PNG, transparent background where practical;
- thumbnail: 640 x 400 PNG;
- icon: 128 x 128 PNG.

App/product branding should not be baked into pedal artwork. The artwork may contain the pedal model name and control legends. Interactive knobs and switches are rendered by the UI rather than permanently drawn into the faceplate.

Artwork paths are optional at the parser level so packages can be developed before final artwork is approved. A future library-validation pass may enforce asset existence for release-ready packages.

## Controls

Control positions use normalized coordinates so layouts are resolution-independent:

```json
"controls": [
  { "id": "OUTPUT", "label": "OUTPUT", "type": "knob", "x": 0.14, "y": 0.20 },
  { "id": "EQ",     "label": "EQ",     "type": "knob", "x": 0.38, "y": 0.20 },
  { "id": "PINCH",  "label": "PINCH",  "type": "knob", "x": 0.62, "y": 0.20 },
  { "id": "WOOL",   "label": "WOOL",   "type": "knob", "x": 0.86, "y": 0.20 }
]
```

- `x = 0` is the left edge and `x = 1` is the right edge.
- `y = 0` is the top edge and `y = 1` is the bottom edge in package metadata.
- supported v0.1 types are `knob`, `switch` and `toggle`;
- `size` is optional and defaults to `1.0`.
- control ids must be unique within a package and should match the `.cpedal` live control ids.

## UI metadata

```json
"ui": {
  "faceplateAspectRatio": 1.6,
  "controlStyle": "knob_overlay"
}
```

The aspect ratio describes the intended artwork display region. `controlStyle` is a hint for the presentation layer and does not alter DSP behaviour.

## Presets

`presets` is an optional list of package-relative JSON files:

```json
"presets": [
  "presets/default.json"
]
```

A simple preset is:

```json
{
  "name": "Default",
  "values": {
    "OUTPUT": 0.50,
    "EQ": 0.50,
    "PINCH": 0.50,
    "WOOL": 0.50
  }
}
```

Values are normalized from `0.0` to `1.0`.

## Build and bundle behaviour

The macOS bundle copies the complete `pedals/` directory to `Contents/Resources/pedals`. Directory structure is preserved so relative manifest paths remain stable. Existing `.cpedal` library loading remains available during migration and external `.cpedal` loading remains an engineering feature.

## Migration rule

A circuit without a package remains valid and continues to use the generic Circuit Lab presentation. Packages are additive. Pedals should be migrated one at a time after their visual assets and control layout have been reviewed.

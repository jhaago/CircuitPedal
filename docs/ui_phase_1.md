# CircuitPedal UI Phase 1

This branch establishes the first visual redesign pass for the native macOS Circuit Lab.

## Scope

- Dark industrial desktop shell inspired by the approved CircuitPedal concepts.
- Clear three-column hierarchy: pedal library / audio I/O, central virtual pedal, signal / engine status.
- Rotary native controls for continuous pedal parameters.
- Grid-based generic circuit controls with discrete switch support preserved.
- Footswitch-style bypass control and clearer live-audio state.
- Segmented input/output meters with dB readouts.
- Existing model selection, external `.cpedal` loading, Core Audio setup and real-time engine behaviour preserved.
- Stable generic live path remains 1x; no DSP architecture changes are part of this UI branch.

## Deliberately deferred

- Schematic editor / circuit view.
- Component browser and editable component values.
- Spectrum, waveform or oscilloscope views.
- Preset browser and pedalboard chains.
- Photorealistic per-model pedal artwork.
- Animato, Blueberry and Tentacle promotion into the stable bundled library.

The goal of Phase 1 is to replace the crude engineering-form layout with a coherent visual foundation without mixing UI work with audio-engine changes.

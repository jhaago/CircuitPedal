# CircuitPedal UI Phase 1 — Concept Shell

This branch establishes the first major visual foundation for the native macOS Circuit Lab and moves the app toward the premium CircuitPedal concept-art direction without changing the accepted audio/DSP path.

## Implemented shell

- Permanent dark top application bar with CircuitPedal branding and live-audio status.
- Left library / audio-I/O sidebar containing the existing circuit chooser, external-file workflow, device selection, input channel, buffer selection, and start/stop controls.
- Dominant central pedal workspace that gives the loaded effect a proper visual identity instead of presenting it as a developer form.
- Rotary native controls for continuous parameters and discrete popup controls for switches.
- Persistent right-side signal / engine inspector with input and output meters, dB readouts, runtime status, and the accepted 1x live-path indication.
- Bottom status/performance strip reserved for stage-oriented feedback and future performance controls.
- Footswitch-style active/bypass control with clearer live-state presentation.
- Coherent dark graphite palette, restrained warm accent, muted secondary text, and status colours.
- Existing built-in circuit selection, `Load External…`, generic `.cpedal` controls, Core Audio configuration, metering, bypass and live processing behaviour preserved.

## Design intent

Phase 1 establishes the application frame for the longer-term concept:

`top bar / left library / centre workspace / right inspector / bottom performance area`

The centre should remain the visual priority. Future work can replace the generic pedal face with package-provided artwork and can turn the centre header into a true signal-chain editor without another wholesale application-shell rewrite.

## Deliberately deferred to later phases

- Full drag-and-drop multi-effect signal chain.
- Package-aware pedal/preset browser with search, categories and favourites.
- Per-pedal artwork and richer metadata-driven control positioning.
- Dedicated Parameters / Circuit / EQ / Settings inspector tabs.
- Spectrum, waveform and oscilloscope views.
- Scene/footswitch slots and preset save/navigation controls.
- Schematic/component editor.
- Photorealistic materials or heavy animation.
- Promotion of Animato, Blueberry or Tentacle into the stable bundled library.

## Engineering constraints

- The stable generic live path remains 48 kHz / 1x.
- No pedal-model values or DSP architecture are changed by this UI branch.
- Source-draft pedals remain withheld from the normal bundled library.
- Visual polish must not replace functioning controls with non-functional mockups.

The goal is a credible, significantly more polished foundation that can be physically tested on macOS before Phase 2 expands the signal-chain/pedalboard experience.

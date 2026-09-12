# CircuitPedal pedal packages

Each subfolder represents one user-facing pedal package. Packages add metadata, artwork references, UI layout and presets around the existing `.cpedal` electrical model.

The electrical circuit files remain authoritative in `circuits/` during the migration period.

Artwork-enabled packages currently include Woolly Mammoth and the NYC, Triangle,
Ram's Head and Green Russian Big Muff variants. Each package declares separate
hero-pedal, hero-background and signal-chain assets in its manifest.

See `docs/pedal_package_format.md` for the v0.1 package format.

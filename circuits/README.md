# CircuitPedal circuit files

The `circuits/` folder contains human-readable `.cpedal` models used by the generic circuit engine.

- `two_transistor_fuzz_demo.cpedal` is a deliberately generic two-stage fuzz
  used to exercise file loading and live controls.
- `woolly_mammoth_reference_draft.cpedal` is the first live-tested real-world target.
- `big_muff_triangle.cpedal`, `big_muff_rams_head.cpedal`,
  `big_muff_green_russian.cpedal` and `big_muff_nyc.cpedal` are reconstructed
  from the supplied Big Muff project schematic and its per-version BOM.
- `naga_viper.cpedal` follows the published Naga Viper / Aion Hydra signal-path
  schematic and exposes Range, Boost and Heat.
- `fuzz_factory_reference.cpedal` is the first five-control germanium/PNP
  reference and exposes Stab, Gate, Comp, Drive and Volume.
- `fat_fuzz_factory_reference.cpedal` extends that topology with a live
  three-position Fat switch and switchable parallel interstage capacitors.
- `fuzzolo_reference_draft.cpedal` models the two-2N3904 fuzz core, BS170
  output booster, Pulse Width/Volume controls and a live Passive/Active pickup selector.
- `two_transistor_fuzz_demo.cpedal` remains a generic solver demonstration.

The Woolly file is deliberately labelled **Reference Draft**. CircuitPedal's
current 2N3904 is a compact Ebers-Moll approximation, and the circuit has not yet
been fitted against measured hardware or a manufacturer SPICE model. The file is
therefore suitable for architecture and listening tests, not a claim of exact
commercial-pedal reproduction.

## Circuit file syntax

```text
CPEDAL 1
NAME "Example"

V VCCSRC VCC 0 9
AUDIO GUITAR INPUT 0 0.2

R R1 VCC N1 51k
C C1 INPUT B1 220n
Q Q1 C B E 2N3904
D D1 CLIP 0 1N4148
POT GAIN 0 WIPER SIGNAL 100k LIN 0.5

OUTPUT OUT 1
```

Engineering notation such as `4k99`, `2k2`, `220n`, `100u` and `1M`
is accepted. Comments may start with `#` or `//`.

V0.9 also supports PNP BJTs, N-channel JFETs, N-channel MOSFETs and early
generic op-amps in circuit files. See `docs/circuit_file_format.md` for the
full syntax and `docs/model_coverage.md` for the current conversion backlog.

Potentiometers become named live controls in the macOS GUI. The V0.12 GUI shows up to sixteen controls in a scrollable panel. `POT_LINK`
allows one control to move multiple electrical pot sections together, and
`SWITCH` exposes labelled SPST/SPDT/on-off-on controls.

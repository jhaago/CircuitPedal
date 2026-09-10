# CircuitPedal circuit files

The `circuits/` folder contains human-readable `.cpedal` models used by the generic circuit engine.

## Stable macOS bundle

The current stable macOS app deliberately bundles only models that have either
passed physical listening checks or are retained as established internal test
fixtures. Live generic processing is currently fixed at the physically validated
1x path; 4x oversampling remains engineering work rather than a production mode.

Physically accepted/usable models at this checkpoint:

- `woolly_mammoth_reference_draft.cpedal`
- `big_muff_triangle.cpedal`
- `big_muff_rams_head.cpedal`
- `big_muff_green_russian.cpedal`
- `big_muff_nyc.cpedal`
- `fuzz_factory_reference.cpedal`
- `fat_fuzz_factory_reference.cpedal`
- `fuzzolo_reference_draft.cpedal`
- `ts10_reference_draft.cpedal`
- `kalamazoo_reference_draft.cpedal`
- `naga_viper.cpedal`

`two_transistor_fuzz_demo.cpedal` remains bundled as an internal generic-engine
exercise circuit rather than a commercial-pedal emulation.

## Experimental source drafts

These files remain in source so their engineering work is not lost, but CMake
explicitly excludes them from the stable macOS app until they pass physical
listening checks:

- `animato_reference_draft.cpedal`
- `blueberry_bass_overdrive_reference_draft.cpedal`
- `eqd_tentacle_reference_draft.cpedal`

A model being parser/solver-stable is not enough to promote it into the stable
bundle; physical listening remains the acceptance gate for live pedal behaviour.

## Model notes

- `woolly_mammoth_reference_draft.cpedal` is the first live-tested real-world target.
- The four Big Muff files are reconstructed from the supplied Big Muff project schematic and its per-version BOM.
- `naga_viper.cpedal` follows the published Naga Viper / Aion Hydra signal-path schematic and exposes Range, Boost and Heat.
- `fuzz_factory_reference.cpedal` exposes Stab, Gate, Comp, Drive and Volume.
- `fat_fuzz_factory_reference.cpedal` extends that topology with a live three-position Fat switch.
- `fuzzolo_reference_draft.cpedal` models the two-2N3904 fuzz core, BS170 output booster, Pulse Width/Volume controls and a Passive/Active pickup selector.
- `ts10_reference_draft.cpedal` models the engaged TS10 clipping, tone, level and transistor-buffer signal path.
- `kalamazoo_reference_draft.cpedal` reconstructs the Lovepedal Kalamazoo engaged path with dual JRC4558 sections, Tone/Glass networks and four live controls.

Files labelled **Reference Draft** may have passed CircuitPedal's parser/solver
and end-to-end stability checks without being component-accurate clones. Physical
listening acceptance means the current model is useful enough for the live test
library; it does not imply manufacturer-level measured/SPICE equivalence.

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

Engineering notation such as `4k99`, `2k2`, `220n`, `100u` and `1M` is accepted.
Comments may start with `#` or `//`.

The format supports PNP BJTs, N-channel JFETs, N-channel MOSFETs, dynamic generic
op-amps, linked/multi-gang potentiometers and live circuit switches. See
`docs/circuit_file_format.md` for the full syntax and `docs/model_coverage.md`
for the conversion backlog.

Potentiometers and switches become named live controls in the macOS GUI. The GUI
shows up to sixteen controls in a scrollable panel. `POT_LINK` allows one control
to move multiple electrical pot sections together, while `SWITCH_LINK` allows
mechanically linked multi-pole switches to follow one GUI control.

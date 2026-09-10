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
- `ts10_reference_draft.cpedal` is the first dual-op-amp library target and
  models the engaged TS10 clipping, tone, level and transistor-buffer signal path.
- `animato_reference_draft.cpedal` reconstructs the Human Gear Animato signal
  path with NTE102/NTE103 germanium front end, 2SC2240 Muff-derived stages,
  dual-gang Distortion and linked Bias switching.
- `kalamazoo_reference_draft.cpedal` reconstructs the Lovepedal Kalamazoo
  engaged path with dual JRC4558 sections, series-pair silicon feedback clipping,
  separate Tone/Glass networks and four live controls.
- `blueberry_bass_overdrive_reference_draft.cpedal` reconstructs the BearFoot
  Blueberry Bass Overdrive with a CA3130EZ voltage-amplifier stage, red-LED
  feedback clipping, 1N4007 post-stage clamping, a 2N5457 output stage and
  live Drive, Tone and Volume controls.
- `eqd_tentacle_reference_draft.cpedal` reconstructs the zero-control,
  three-transistor analog octave-up path with its complementary phase splitter
  and dual-diode full-wave rectifier.

Files labelled **Reference Draft** have passed CircuitPedal's parser/solver and
end-to-end stability checks but have not yet been fitted against measured hardware
or a manufacturer-quality SPICE reference. They are suitable for architecture and
listening tests, not claims of exact commercial-pedal reproduction.

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

The format supports PNP BJTs, N-channel JFETs, N-channel MOSFETs, dynamic
generic op-amps, linked/multi-gang potentiometers and live circuit switches.
See `docs/circuit_file_format.md` for the full syntax and
`docs/model_coverage.md` for the current conversion backlog.

Potentiometers and switches become named live controls in the macOS GUI. The
GUI shows up to sixteen controls in a scrollable panel. `POT_LINK` allows one
control to move multiple electrical pot sections together, while `SWITCH_LINK`
allows mechanically linked multi-pole switches to follow one GUI control.

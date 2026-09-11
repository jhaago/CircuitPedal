# CircuitPedal native approved-layout pass

This branch ports the tablet-reviewed HTML design direction into the real macOS Circuit Lab application.

Implemented in the native app:

- approved top/left/centre/right/bottom visual hierarchy;
- richer atmospheric selected-pedal hero treatment;
- compact signal-chain overview with an **Edit Signal Chain** mode;
- interactive native routing-design canvas supporting draggable nodes and split/recombine cable creation;
- permanent inspector tabs for **Parameters / Circuit / EQ / Settings**;
- real master **Input Trim** and **Output Level** gain stages in the macOS audio callback;
- live input/output metering retained;
- current device/channel/buffer, circuit loading, bypass and parameter-control paths preserved.

## Important routing boundary

The routing editor is currently a native interaction/design layer only. The live engine still processes the single selected circuit/model. This is shown explicitly in the routing view so the UI does not imply that multi-effect split/merge DSP is already active.

The master input/output controls are real audio gain stages. Input Trim is limited to -18 dB to +18 dB and Output Level to -18 dB to +6 dB.

No oversampling changes were made; the stable live path remains 48 kHz / 1x.

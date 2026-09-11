# UI Phase 1 physical test notes

When testing `feature/circuit-lab-ui` on macOS, verify both function and presentation.

## Functional regression checks

- app launches from `build_and_run_gui.command`;
- stable bundled pedal list is unchanged;
- Built-in Distortion+ controls work;
- generic circuit knobs and switches update correctly;
- Start / Stop Audio works with the selected interface, channel and buffer;
- Bypass/Active toggles correctly while audio is running;
- input/output segmented meters and dB readouts respond;
- model switching remains locked while audio is running;
- external `.cpedal` loading still works while stopped;
- switching between bundled circuits still works without relaunching;
- no change in the accepted 48 kHz / 1x live generic tone compared with stable `main`.

## Visual / UX checks

- top bar, left sidebar, central pedal workspace, right inspector and bottom status strip are visually distinct and balanced;
- loaded pedal name is easy to identify at a glance;
- continuous controls are readable and comfortable to manipulate;
- switch controls remain obvious when a model exposes them;
- active/stopped audio state is unambiguous;
- no text or controls overlap at the default window size;
- UI remains usable on the older test Mac without obvious redraw lag or excessive CPU use;
- dark-theme contrast remains readable in a dim room.

## Not acceptance criteria yet

Do not reject Phase 1 merely because it does not yet include the full concept-art signal chain, preset browser, spectrum display, per-pedal artwork, inspector tabs or scene footswitches. Those are later phases built on this shell.

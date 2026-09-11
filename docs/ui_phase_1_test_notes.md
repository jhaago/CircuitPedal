# UI Phase 1 test notes

When physically testing this branch on macOS, verify:

- app launches from `build_and_run_gui.command`;
- stable bundled pedal list is unchanged;
- Built-in Distortion+ controls work;
- generic circuit knobs and switches update live;
- Start / Stop Audio works with the selected interface, channel and buffer;
- Bypass toggles correctly while audio is running;
- input/output meters and dB readouts respond;
- model switching remains locked while audio is running;
- external `.cpedal` loading still works while stopped;
- no change in the accepted 1x live generic tone compared with stable main.

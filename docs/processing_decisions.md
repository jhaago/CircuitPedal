# CircuitPedal Processing Decisions

This document records processing-path decisions that should not be lost when temporary validation/fix branches are retired.

## 2026-09-11 — 48 kHz / 1x is the accepted live path

CircuitPedal's current production/live generic-circuit path remains **48 kHz / 1x**.

Physical A/B testing of the extracted runtime diagnostics showed:

- the 1x path is the known-good listening baseline;
- the 4x path still sounds materially wrong;
- neither path produced host solve failures during the physical test;
- neither path produced failed oversampled sub-solves during the physical test;
- the 4x problem therefore was not explained by obvious nonlinear-solver failure during that session.

The 4x oversampling work is **deferred**, not accepted. The experimental `fix/oversampling-regression` branch should not be merged into `main` solely because it contains a plausible DSP correction; physical sound quality remains the acceptance criterion for that workstream.

If oversampling is revisited later, start a fresh purpose-named branch from the then-current `main`, reproduce the current 4x failure, and use the historical branch/commits only as engineering reference.

## Woolly Mammoth validation consequence

The Woolly Mammoth acceptance campaign is evaluated on the accepted 48 kHz / 1x path. Its automated acceptance harness, physical checklist and dated baselines are retained in the repository as the template for future pedal-model validation.

## Runtime diagnostics retention

The temporary `validation/live-runtime` branch served its purpose and should not be merged wholesale. The reusable dependency-free `validation/model_signal_summary.py` utility is retained in `main`; the floating diagnostic overlay and experimental runtime mode switching remain validation-only history unless a future debugging need justifies rebuilding them from current `main`.

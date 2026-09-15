# Human Gear Animato Model Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make CircuitPedal's Human Gear Animato model technically sound, measurable, stable and circuit-faithful across guitar and bass ranges without adding non-original bass-preservation behaviour.

**Architecture:** Keep the existing `.cpedal` component-level model and generic circuit engine. First add a dedicated 1x validation campaign that measures the current model at controlled control positions and sample rates. Use those measurements plus the traced Animato/Aion Polaris circuit to make only confirmed model-file corrections, then rerun the full project tests and the focused campaign. Do not change shared DSP unless a failing diagnostic proves the shared engine is wrong.

**Tech Stack:** C++17 generic circuit solver, `.cpedal` netlists, `circuitpedal_validate`, Python 3 validation runner, CMake/CTest, GitHub Actions.

**Spec:** User request in this conversation; reference evidence from the traced Human Gear Animato circuit and Aion FX Polaris documentation.

## Global Constraints

- Work from the current CircuitPedal implementation; do not create a new project or redesign the application.
- Do not merge or delete branches.
- Do not alter unrelated pedal DSP.
- Treat the Animato as a guitar/bass-capable distortion/fuzz circuit, not as a bass-only effect.
- Do not add clean blend, bass-preservation EQ or other features absent from the traced circuit.
- Use the accepted macOS live generic path: 48 kHz / 1x. Also exercise 44.1 and 96 kHz for sample-rate robustness.
- Keep the Animato source-only until automated validation passes; physical listening acceptance remains a separate final gate.
- Preserve uncertainties around original transistor spread, germanium leakage, factory trimmer position and production revision differences rather than inventing values.

---

### Task 1: Establish a repeatable Animato diagnostic campaign

**Files:**
- Create: `validation/animato/run_acceptance.py`
- Create: `validation/animato/README.md`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: `build/circuitpedal_validate render`, the current `circuits/animato_reference_draft.cpedal`, and its controls `BOOST`, `DISTORTION`, `TONE`, `VOLUME`, `BIAS`.
- Produces: `build/validation/animato-acceptance/acceptance_report.md` plus per-case CSV renders; process exit status is the CI gate.

- [x] **Step 1: Add a failing behavioural gate for BOOST direction**

The pre-fix model failed exactly as predicted: BOOST 0 produced 0.231211295 V AC RMS at `BOOST_W`, while BOOST 1 collapsed to 0.000000025 V.

- [x] **Step 2: Add non-gating baseline measurements before changing DSP**

Peak, RMS, host clipping, harmonic content, symmetry, bias state, Tone states, bass/guitar frequencies, output-domain calibration and sample-rate results are emitted into the acceptance report.

- [x] **Step 3: Add hard numerical-safety gates**

The campaign fails on non-finite output, convergence failures, loss of meaningful signal where signal is expected, host full-scale clipping inside the defined normal/high-output envelope, incorrect BOOST direction, output-conversion inconsistency, transient stress failures or sample-rate robustness failures. Separate all-controls-maximum cases document the output boundary without redefining a 9 V pedal's physical Volume attenuator as a DSP limiter.

- [x] **Step 4: Add the acceptance runner to CI**

Linux CI runs the campaign and uploads the report/CSV artifact.

- [x] **Step 5: Run CI and verify RED**

Run 349 built successfully and all existing CTests/other pedal checks passed; only the new Animato BOOST-direction gate failed.

### Task 2: Correct confirmed control-orientation error

**Files:**
- Modify: `circuits/animato_reference_draft.cpedal`

**Interfaces:**
- Consumes: the pot convention in `GenericCircuit` where normalized 0 is terminal 1 and normalized 1 is terminal 3, plus the traced/Aion schematic.
- Produces: BOOST increasing clockwise/user-normalized position increases the Rangemaster contribution instead of moving the wiper toward AC ground.

- [x] **Step 1: Make the minimum production change**

Only the BOOST outer terminals were swapped: `PAIR_C ... VA` became `VA ... PAIR_C`. The 10k value, LIN taper and 0.65 default were retained.

- [x] **Step 2: Run focused validation**

The fixed model produces 0.000000025 V at BOOST 0 and 0.231211295 V at BOOST 1. The focused acceptance gate passes.

- [x] **Step 3: Inspect before/after metrics**

The correction reverses BOOST action without changing Tone, Bias, clipping topology, Distortion law or output conversion.

### Task 3: Diagnose gain staging and output-domain calibration

**Files:**
- Modify if justified: `validation/animato/run_acceptance.py`
- Modify only if proved: `circuits/animato_reference_draft.cpedal`

**Interfaces:**
- Consumes: measured output/node levels from Task 1/2, `AUDIO GUITAR ... 0.20`, `OUTPUT` conversion semantics, physical finding that low DISTORTION was excessively quiet, and the traced component values.
- Produces: a documented decision to either retain the current output conversion or correct it, with quantitative evidence.

- [x] **Step 1: Measure analog-domain versus digital-domain level**

At medium drive, a 50 mV-peak analogue input produces 0.410594 V RMS at `OUT` and 0.205297 FS RMS at the host, exactly 0.5 FS/V, with +21.30 dB analogue RMS gain.

- [x] **Step 2: Add a failing level/calibration test only if evidence proves a defect**

No output-calibration defect was found, so no artificial failing constant was added. The campaign instead gates conversion consistency and host clipping/headroom through 150 mV-peak input with Volume at 75%, and reports all-controls-maximum behavior separately.

- [x] **Step 3: Apply the smallest calibration correction if the test fails**

Not applicable: the evidence supports retaining `OUTPUT OUT 0.5`. Copying another pedal's larger conversion would create host hard clipping.

- [x] **Step 4: Verify hard-strum behaviour**

A 150 mV-peak high-drive sine with Volume at 75%, step, impulse and two dual-tone stress cases all remained finite, convergent and at 0% host full-scale clipping. An all-controls-maximum 55 Hz case exceeds the 2 V-peak host mapping and is retained as a diagnostic because the real Volume control is the intended output attenuator. Physical pickup/strum retest remains required.

### Task 4: Validate Distortion, Tone, Bias and circuit fidelity

**Files:**
- Modify if required by evidence: `validation/animato/run_acceptance.py`
- Modify only if traced evidence/tests prove an error: `circuits/animato_reference_draft.cpedal`

**Interfaces:**
- Consumes: Aion/trace values (dual 100kA Distortion, 100kB Tone/Volume, 1N914 feedback clipping, 10n input coupling, Sziklai front end), baseline measurements, and physical observations.
- Produces: quantified control sweeps and any narrowly justified component/taper/orientation corrections.

- [x] **Step 1: Quantify the dual-gang DISTORTION sweep**

Measured output relative to maximum is about -70.84 dB at 10%, -18.68 dB at 25%, -2.47 dB at 50%, -0.53 dB at 75% and 0 dB at 100%. The steep sweep follows the traced dual 100kA topology, so it was retained.

- [x] **Step 2: Quantify frequency response**

Low-level output gain measures -9.17 dB at 40 Hz, +8.00 dB at 82 Hz, +24.35 dB at 196 Hz, +30.53 dB at 440 Hz, +29.01 dB at 1 kHz, +2.26 dB at 5 kHz and -13.61 dB at 10 kHz. The measured bass loss is consistent with the Rangemaster front end and Aion's bass-use warning.

- [x] **Step 3: Quantify clipping/harmonics**

Medium/high drive produce strong predominantly odd harmonic content with small peak asymmetry and zero host hard clipping in the controlled cases; the two antiparallel 1N914 feedback-clipping stages remain unchanged.

- [x] **Step 4: Check BIAS effect**

Both linked poles remain intact. The alternate state produces a modest level/harmonic shift rather than instability or a catastrophic gain change, consistent with the traced circuit description.

- [x] **Step 5: Change nothing without a failed evidence-backed test**

No further model-DSP defect was demonstrated after the BOOST correction. No speculative EQ, bass blend, taper, clipping or transistor changes were made.

### Task 5: Full verification and engineering handoff

**Files:**
- Update: `validation/animato/README.md`
- Update if needed: `validation/draft_pedals/2026-09-11_physical_findings.md`

**Interfaces:**
- Consumes: final branch state and all validation artifacts.
- Produces: reproducible verification instructions and a physical listening checklist for macOS.

- [ ] **Step 1: Run complete CMake build and CTest in CI**

Linux and macOS normal build/test jobs and the unrelated ngspice/Woolly regression job are green after the DSP fix and expanded diagnostics. Final sanitizer/final-HEAD status is checked again immediately before handoff.

- [x] **Step 2: Run Animato focused acceptance**

The focused campaign passes at 44.1/48/96 kHz with finite output, zero solver failures, meaningful guitar/bass-range output, correct BOOST direction and no host clipping in the defined stress cases.

- [x] **Step 3: Confirm unrelated models are unaffected**

No shared DSP file was changed. Existing CTest, Blueberry checks, Woolly acceptance and ngspice reference campaign remain green.

- [x] **Step 4: Keep physical-listening promotion separate**

The Animato bundle exclusion remains in place. The validation README contains the exact Mac listening checklist for BOOST, DISTORTION, TONE, VOLUME, BIAS, guitar, bass and hard-input behaviour.

- [ ] **Step 5: Final report**

Report original implementation, faults, real-circuit facts/inferences/unknowns, changes, quantitative verification, guitar-vs-bass behaviour, remaining uncertainty and the Mac listening checklist.

## Independent review addendum — 2026-09-14

The follow-up review independently re-read the schematic, pot implementation,
component models, live audio route and generated measurements rather than taking
the report above as authoritative.

- The BOOST correction is confirmed from both the traced schematic and
  `GenericCircuit` terminal semantics.
- The `.cpedal` values and topology remain defensible; no further netlist change
  is supported by the available evidence.
- The exact original dual 100kA taper curve is not established. The steep model
  sweep is a consequence of the confirmed two-attenuator topology plus
  CircuitPedal's plausible 10%-at-midpoint law, not proof of an exact physical
  control match.
- The prior 180 ms steady-sine reports are superseded. The final 10 uF coupling
  capacitor needs seconds to settle after signal onset, so the new campaign uses
  four-second renders and analyzes the final coherent second with DC removed.
- The accepted 1x live path incorrectly delayed dry/bypass audio and reported the
  experimental 4x FIR's 47-sample latency. A focused failing test preceded the
  runtime-mode-aware correction (0 samples at 1x; 47 at 4x).
- 4x processing measurably reduces an extreme aliasing probe, but it remains
  deferred because the current path failed physical listening and clamps/converts
  circuit output before decimation.

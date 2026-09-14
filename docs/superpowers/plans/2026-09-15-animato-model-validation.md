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

- [ ] **Step 1: Add a failing behavioural gate for BOOST direction**

Render matched 48 kHz / 1x cases with BOOST at `0` and `1`, DISTORTION/TONE/VOLUME held constant, and assert that the user-facing higher BOOST position produces materially higher RMS signal at the Rangemaster output / subsequent signal path. The current model is expected to fail because terminal 3 of BOOST is the 9 V AC-ground rail.

- [ ] **Step 2: Add non-gating baseline measurements before changing DSP**

Report peak, RMS, clipping percentage and solver failures for low/mid/high DISTORTION, both BIAS states, Tone extremes, guitar-range and bass-range sine inputs, and 44.1/48/96 kHz sample rates. Include internal nodes around the booster and clipping stages where useful.

- [ ] **Step 3: Add hard numerical-safety gates**

Fail if any render has non-finite output, solver convergence failures, no meaningful signal, or unexpected full-scale digital clipping in the normal instrument-level test cases.

- [ ] **Step 4: Add the acceptance runner to CI**

Run the script on Linux after building `circuitpedal_validate`; upload/report artifacts if practical. Do not bundle/promote the Animato as part of this task.

- [ ] **Step 5: Run CI and verify RED**

Expected result: the focused Animato job fails specifically on BOOST direction while baseline metrics are still emitted. A compile/configuration error is not an acceptable RED result.

### Task 2: Correct confirmed control-orientation error

**Files:**
- Modify: `circuits/animato_reference_draft.cpedal`

**Interfaces:**
- Consumes: the pot convention in `GenericCircuit` where normalized 0 is terminal 1 and normalized 1 is terminal 3, plus the traced/Aion schematic.
- Produces: BOOST increasing clockwise/user-normalized position increases the Rangemaster contribution instead of moving the wiper toward AC ground.

- [ ] **Step 1: Make the minimum production change**

Swap only the BOOST outer terminals so the signal node is terminal 3 and the 9 V rail is terminal 1. Do not change the 10k value, linear taper or initial setting in the same commit.

- [ ] **Step 2: Run focused validation**

Expected result: the previously failing BOOST-direction gate passes; stability gates remain green.

- [ ] **Step 3: Inspect before/after metrics**

Confirm that the change affects direction rather than silently changing unrelated Tone, BIAS or output behaviour.

### Task 3: Diagnose gain staging and output-domain calibration

**Files:**
- Modify if justified: `validation/animato/run_acceptance.py`
- Modify only if proved: `circuits/animato_reference_draft.cpedal`

**Interfaces:**
- Consumes: measured output/node levels from Task 1/2, `AUDIO GUITAR ... 0.20`, `OUTPUT` conversion semantics, physical finding that low DISTORTION was excessively quiet, and the traced component values.
- Produces: a documented decision to either retain the current output conversion or correct it, with quantitative evidence.

- [ ] **Step 1: Measure analog-domain versus digital-domain level**

Use exported output/internal node voltages and digital `output_fs` to determine whether the low-drive level problem is circuit behaviour or only model-I/O scaling. Record the actual conversion ratio rather than assuming that every pedal must use the same `OUTPUT` multiplier.

- [ ] **Step 2: Add a failing level/calibration test only if evidence proves a defect**

The test must describe observable behaviour at realistic instrument level (for example, pathological attenuation unrelated to the analogue node voltage). Do not assert a configuration string merely to force a desired constant.

- [ ] **Step 3: Apply the smallest calibration correction if the test fails**

Change only `OUTPUT` scaling if the analog circuit is healthy and the digital-domain conversion is the defect. Do not compensate with arbitrary gain blocks or EQ.

- [ ] **Step 4: Verify hard-strum behaviour**

Render higher-amplitude guitar-like and bass-like transients/sines and confirm the correction does not simply replace low output with pervasive digital hard clipping.

### Task 4: Validate Distortion, Tone, Bias and circuit fidelity

**Files:**
- Modify if required by evidence: `validation/animato/run_acceptance.py`
- Modify only if traced evidence/tests prove an error: `circuits/animato_reference_draft.cpedal`

**Interfaces:**
- Consumes: Aion/trace values (dual 100kA Distortion, 100kB Tone/Volume, 1N914 feedback clipping, 10n input coupling, Sziklai front end), baseline measurements, and physical observations.
- Produces: quantified control sweeps and any narrowly justified component/taper/orientation corrections.

- [ ] **Step 1: Quantify the dual-gang DISTORTION sweep**

Measure output RMS and harmonic content at low/mid/high settings. Keep the documented dual 100k audio topology unless the implementation's taper/orientation demonstrably disagrees with the traced circuit.

- [ ] **Step 2: Quantify frequency response**

Use low-amplitude sweeps or multi-frequency renders at low drive to characterize the 10n Rangemaster input loss and Big-Muff-style Tone network. Confirm bass fundamentals are naturally reduced rather than artificially restored.

- [ ] **Step 3: Quantify clipping/harmonics**

At medium/high drive, inspect symmetry and harmonic content from the two antiparallel 1N914 feedback-clipping stages. Confirm no extra generic clipper or dry blend is present.

- [ ] **Step 4: Check BIAS effect**

Verify both linked switch poles move together, remain stable, and cause the expected modest bias/EQ/gain shift rather than a catastrophic level change.

- [ ] **Step 5: Change nothing without a failed evidence-backed test**

If the traced topology already matches and the observed sensitivity is inherent to the dual audio pot, document that result rather than "improving" it arbitrarily.

### Task 5: Full verification and engineering handoff

**Files:**
- Update: `validation/animato/README.md`
- Update if needed: `validation/draft_pedals/2026-09-11_physical_findings.md`

**Interfaces:**
- Consumes: final branch state and all validation artifacts.
- Produces: reproducible verification instructions and a physical listening checklist for macOS.

- [ ] **Step 1: Run complete CMake build and CTest in CI**

Require Linux and macOS project builds/tests to pass with no new compiler errors; sanitizer CI must remain green.

- [ ] **Step 2: Run Animato focused acceptance**

Require stability at 44.1/48/96 kHz, finite output, zero solver failures, functional controls, meaningful guitar/bass output and correct BOOST direction.

- [ ] **Step 3: Confirm unrelated models are unaffected**

Because the intended fix is model-local, the existing full test suite is the regression guard. If a shared file was changed, add explicit regression evidence for known-good pedals.

- [ ] **Step 4: Keep physical-listening promotion separate**

Do not remove the Animato bundle exclusion unless the user explicitly chooses to promote it after Mac A/B/listening. Document exact listening positions for BOOST, DISTORTION, TONE, VOLUME and BIAS and both guitar/bass material.

- [ ] **Step 5: Final report**

Report original implementation, faults, real-circuit facts/inferences/unknowns, changes, quantitative verification, guitar-vs-bass behaviour, remaining uncertainty and the Mac listening checklist.

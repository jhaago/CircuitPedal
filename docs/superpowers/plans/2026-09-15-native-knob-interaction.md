# Native Knob Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make macOS rotary controls smooth and predictable with vertical dragging, and make package hero artwork reflect authoritative pedal-control values.

**Architecture:** Keep interaction and angle calculations in a small portable C++ helper so behavior can be regression-tested without AppKit. `CPPremiumKnobCell` translates AppKit pointer events through that helper. `CircuitPedalHeroView` receives a display-only snapshot built from `MacAudioEngine`, while `PedalPackageArtwork` matches snapshot values to manifest controls by canonical ID and renders overlays.

**Tech Stack:** C++17, Objective-C++, AppKit, CMake/CTest

**Spec:** Approved design in the 2026-09-15 CircuitPedal conversation: vertical drag, Shift fine adjustment, corrected 270-degree sweep, ID-matched live hero knob indicators, no interactive hero controls.

## Global Constraints

- Work on `fix/native-knob-interaction`, based on `origin/feature/native-ui-feedback-pass`.
- Do not alter pedal DSP behavior or introduce a second authoritative state model.
- Hero artwork remains display-only and reads values originating from `MacAudioEngine`.
- Match package controls by ID/name, never by manifest array order.
- Do not merge the branch.

---

### Task 1: Testable knob interaction math

**Files:**
- Create: `src/KnobInteraction.h`
- Create: `src/KnobInteraction.cpp`
- Create: `src/knob_interaction_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `normalizedValueAfterVerticalDrag(double initial, double verticalPoints, bool fineAdjustment)` and `knobAngleDegrees(double normalizedValue)` in namespace `circuitpedal`.

- [ ] **Step 1: Write the failing tests**

```cpp
expectNear(normalizedValueAfterVerticalDrag(0.5, 18.0, false), 0.6);
expectNear(normalizedValueAfterVerticalDrag(0.5, -18.0, false), 0.4);
expectNear(normalizedValueAfterVerticalDrag(0.5, 18.0, true), 0.51);
expectNear(normalizedValueAfterVerticalDrag(0.95, 180.0, false), 1.0);
expectNear(normalizedValueAfterVerticalDrag(0.05, -180.0, false), 0.0);
expectNear(knobAngleDegrees(0.0), 225.0);
expectNear(knobAngleDegrees(0.5), 90.0);
expectNear(knobAngleDegrees(1.0), -45.0);
```

- [ ] **Step 2: Run the focused test and verify RED**

Run: `/root/.local/bin/cmake -S . -B build && /root/.local/bin/cmake --build build --target knob_interaction_test -j2`

Expected: build failure because `KnobInteraction.h` and its functions do not exist.

- [ ] **Step 3: Implement the minimal portable helper**

```cpp
double normalizedValueAfterVerticalDrag(double initial, double verticalPoints, bool fine)
{
    const double scale = fine ? 0.1 : 1.0;
    return std::clamp(initial + verticalPoints * scale / 180.0, 0.0, 1.0);
}

double knobAngleDegrees(double value)
{
    return 225.0 - 270.0 * std::clamp(value, 0.0, 1.0);
}
```

- [ ] **Step 4: Build and run the focused test and verify GREEN**

Run: `/root/.local/bin/cmake --build build --target knob_interaction_test -j2 && ./build/knob_interaction_test`

Expected: exit 0 with all drag, clamp, fine-adjustment, and angle checks passing.

### Task 2: AppKit vertical-drag behavior and corrected drawing

**Files:**
- Modify: `src/NativeAestheticPass.mm`

**Interfaces:**
- Consumes: portable knob functions from Task 1.
- Produces: `CPPremiumKnobCell` behavior where an initial click preserves value, upward movement increases, downward movement decreases, Shift provides 10x precision, and actions remain continuous.

- [ ] **Step 1: Add tracking state and event overrides**

```objective-c
- (BOOL)startTracking:(NSPoint)startPoint at:(NSPoint)currentPoint inView:(NSView*)controlView;
- (BOOL)continueTracking:(NSPoint)lastPoint at:(NSPoint)currentPoint inView:(NSView*)controlView;
- (void)stopTracking:(NSPoint)lastPoint at:(NSPoint)stopPoint inView:(NSView*)controlView mouseIsUp:(BOOL)flag;
```

Store the starting point and normalized starting value. Convert AppKit coordinates so physical upward motion is positive in both flipped and non-flipped views, invoke `normalizedValueAfterVerticalDrag`, then map the normalized result back through the cell's minimum/maximum range.

- [ ] **Step 2: Correct the arc and marker sweep**

Use `knobAngleDegrees(normalized)` so minimum is lower-left, midpoint is top, and maximum is lower-right. Draw the active arc clockwise across the same 270-degree range.

- [ ] **Step 3: Build the focused test again**

Run: `/root/.local/bin/cmake --build build --target knob_interaction_test -j2 && ./build/knob_interaction_test`

Expected: exit 0.

### Task 3: Authoritative live hero knob state

**Files:**
- Modify: `src/main_mac_gui.mm`
- Modify: `src/PedalPackageArtwork.mm`

**Interfaces:**
- Produces: `CircuitPedalHeroView.controlValues`, an immutable dictionary keyed by uppercase engine control name with normalized numeric values.
- Consumes: package manifest `controls` (`id`, `type`, `x`, `y`, `size`) and `knobAngleDegrees`.

- [ ] **Step 1: Add hero display state**

Add a copied `NSDictionary<NSString*, NSNumber*>* controlValues` property whose setter invalidates the hero view.

- [ ] **Step 2: Refresh the snapshot from engine state**

In `refreshModelControls`, rebuild the dictionary from `_circuitControls[i].name` and `_engine->circuitControl(i)`. Rebuild it after a circuit knob or switch action changes engine state.

- [ ] **Step 3: Preserve package manifest controls in the artwork catalog**

Store control ID, type, normalized x/y, and size with each discovered package. Keep lookup keyed by package display name as today.

- [ ] **Step 4: Draw live knob overlays**

After drawing `hero_pedal.png`, iterate manifest controls of type `knob`, look up each value by uppercase ID, convert top-origin normalized manifest coordinates into the drawn pedal rectangle, cover the baked pointer with a compact vector knob cap, and draw its live marker using `knobAngleDegrees`.

- [ ] **Step 5: Build and run all portable tests**

Run: `/root/.local/bin/cmake --build build -j2 && /root/.local/bin/ctest --test-dir build --output-on-failure`

Expected: all tests pass.

### Task 4: macOS verification and branch handoff

**Files:**
- Review: all changed files

**Interfaces:**
- Consumes: completed Tasks 1-3.
- Produces: reviewed commit and unmerged remote branch suitable for physical UI testing.

- [ ] **Step 1: Inspect the final diff**

Run: `git diff --check && git diff --stat && git diff`

Expected: only the knob helper/test, AppKit integration, artwork state integration, CMake wiring, and this plan.

- [ ] **Step 2: Run a native macOS build and tests through CI**

Push `fix/native-knob-interaction`, open a draft PR against `feature/native-ui-feedback-pass`, and wait for the macOS workflow to build `CircuitPedalGUI` and run tests.

- [ ] **Step 3: Report physical-test limitations**

State that mouse feel and per-pedal overlay alignment still need confirmation on a Mac with the packaged artwork; do not claim tactile or visual acceptance from CI alone.

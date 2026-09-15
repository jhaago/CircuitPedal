# CircuitStomp Control Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a portable controller-routing foundation and a macOS CoreMIDI vertical slice in which provisional CircuitStomp P1 relative messages change the first authoritative live pedal control and refresh the GUI.

**Architecture:** Logical controller actions and MIDI decoding live in a portable `circuit_control` library behind a narrow `ControllerTarget`. A macOS target adapter translates that interface to `MacAudioEngine`, while an independent CoreMIDI adapter discovers all sources and posts decoded actions to the main queue. The AppKit host composes these objects and reads engine state back into its controls after successful routing.

**Tech Stack:** C++17, CMake/CTest, Objective-C++17, CoreMIDI, AppKit, existing `MacAudioEngine`

**Spec:** `docs/superpowers/specs/2026-09-15-circuitstomp-control-layer-design.md`

## Global Constraints

- Base `feature/circuitstomp-control-layer` on the documented authoritative `main` commit `129ab2d`.
- Do not modify pedal DSP behavior or validated pedal model files.
- Portable files must not include CoreMIDI, AppKit, or `MacAudioEngine` headers.
- CoreMIDI code must remain in the macOS host layer and must not call UI or engine code from its callback thread.
- CircuitPedal remains the only authoritative state owner.
- Prototype 1 receives only channel-1 CC 20 for P1; do not implement the reserved protocol messages.
- Push the feature branch and leave it unmerged.

---

### Task 1: Portable action, target, routing, and feedback model

**Files:**
- Create: `src/ControllerAction.h`
- Create: `src/ControllerTarget.h`
- Create: `src/ControllerRouter.h`
- Create: `src/ControllerRouter.cpp`
- Create: `src/controller_router_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `using ControllerAction = std::variant<ParameterDelta, ParameterSet, MasterOutputDelta, MasterOutputSet, BypassToggle, BypassSet, StompAction, ExpressionValue>`.
- Produces: `class ControllerTarget` with active-pedal, contextual-parameter, master-output, and bypass getters/setters.
- Produces: `ControllerRouteResult ControllerRouter::route(const ControllerAction&) noexcept` and `ControllerHostState ControllerRouter::hostState() const`.

Define the route result exactly as:

```cpp
struct ControllerRouteResult {
    bool handled = false;
    bool stateChanged = false;
};
```

- [ ] **Step 1: Write the fake target and failing routing tests**

Create `src/controller_router_test.cpp` with a concrete fake target holding five `ControllerParameterState` entries plus master and bypass state. Add separate assertions that:

```cpp
router.route(circuitpedal::ParameterDelta { 0U, 1 });
expect(target.lastParameterIndex == 0U, "P1 delta routes to parameter zero");
expectNear(target.parameters[0].normalizedValue, 0.51f, "P1 increments by its declared step");

router.route(circuitpedal::ParameterDelta { 0U, -1 });
expectNear(target.parameters[0].normalizedValue, 0.50f, "P1 decrements");

router.route(circuitpedal::ParameterSet { 0U, 5.0f });
expectNear(target.parameters[0].normalizedValue, 1.0f, "parameter set clamps high");

router.route(circuitpedal::ParameterSet { 0U, -5.0f });
expectNear(target.parameters[0].normalizedValue, 0.0f, "parameter set clamps low");
```

Loop across indices 1-4 to verify P2-P5 mapping, shrink `parameterCount()` and verify unavailable indices cause no write, label parameter zero `"Output"`, route a `MasterOutputDelta`, and verify only the fake target's master value changes. Test bypass set/toggle, non-finite input rejection, unhandled stomp/expression actions, and `hostState()` values.

- [ ] **Step 2: Register the missing portable target and verify RED**

Add a `controller_router_test` executable linked to a not-yet-populated `circuit_control` target and register it with CTest. Configure/build it and confirm compilation fails because `ControllerAction.h`, `ControllerTarget.h`, and `ControllerRouter.h` do not yet exist.

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target controller_router_test --parallel
```

Expected: build failure for the missing portable controller headers.

- [ ] **Step 3: Implement the minimal portable interfaces**

Define the action structs and variant in `ControllerAction.h`. In `ControllerTarget.h`, define:

```cpp
struct ControllerParameterState {
    bool available = false;
    std::string label;
    float normalizedValue = 0.0f;
    float encoderStepNormalized = 0.01f;
};

struct ControllerHostState {
    std::string selectedPedalName;
    bool bypassed = false;
    std::array<ControllerParameterState, 5> parameters {};
    float masterOutputNormalized = 0.0f;
    std::string presetName;
    std::optional<TunerState> tuner;
};

class ControllerTarget {
public:
    virtual ~ControllerTarget() = default;
    virtual std::string activePedalName() const = 0;
    virtual std::size_t parameterCount() const = 0;
    virtual ControllerParameterState parameterState(std::size_t index) const = 0;
    virtual bool setParameterNormalized(std::size_t index, float value) noexcept = 0;
    virtual float masterOutputNormalized() const noexcept = 0;
    virtual void setMasterOutputNormalized(float value) noexcept = 0;
    virtual bool bypassed() const noexcept = 0;
    virtual void setBypassed(bool value) noexcept = 0;
};
```

Define `TunerState` immediately before `ControllerHostState` with `noteName`, `frequencyHz`, `centsOffset`, and `active`, all initialized safely.

- [ ] **Step 4: Implement minimal router behavior**

Use `std::visit` in `ControllerRouter.cpp`. Clamp finite parameter/master writes with `std::clamp(value, 0.0f, 1.0f)`. For deltas, read the target's current state and use `steps * encoderStepNormalized`; unavailable indices, invalid metadata, failed target writes, non-finite sets, stomp, and expression return `{false, false}`. Return `{true, true}` only when an implemented action changes state, and `{true, false}` for an implemented no-change/clamped action. Build `hostState()` fresh from target getters and copy no more than five contextual parameters.

- [ ] **Step 5: Verify GREEN and regressions**

Run:

```bash
cmake --build build --target controller_router_test --parallel
ctest --test-dir build -R controller_router --output-on-failure
ctest --test-dir build --output-on-failure
```

Expected: all controller-router and existing tests pass.

- [ ] **Step 6: Commit the portable routing slice**

```bash
git add CMakeLists.txt src/ControllerAction.h src/ControllerTarget.h src/ControllerRouter.h src/ControllerRouter.cpp src/controller_router_test.cpp
git commit -m "feat: add portable controller routing"
```

---

### Task 2: Portable provisional MIDI decoder

**Files:**
- Create: `src/CircuitStompProtocol.h`
- Create: `src/CircuitStompProtocol.cpp`
- Create: `src/circuitstomp_protocol_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ControllerAction`, especially `ParameterDelta`.
- Produces: centralized `circuitstomp::prototype1` constants and `std::optional<ControllerAction> decodeMidi1Message(std::uint8_t status, std::uint8_t data1, std::uint8_t data2) noexcept`.

- [ ] **Step 1: Write failing protocol tests**

Test these exact messages independently:

```cpp
const auto up = circuitpedal::circuitstomp::decodeMidi1Message(0xB0, 20, 1);
expectParameterDelta(up, 0U, 1, "B0 14 01 decodes as P1 +1");

const auto down = circuitpedal::circuitstomp::decodeMidi1Message(0xB0, 20, 127);
expectParameterDelta(down, 0U, -1, "B0 14 7F decodes as P1 -1");
```

Also verify values 63 and 65 become `+63` and `-63`, values 0/64 are ignored, and status/channel, non-P1 CC, note-on, and out-of-range data bytes are ignored.

- [ ] **Step 2: Register the test and verify RED**

Add the source to `circuit_control`, create/register `circuitstomp_protocol_test`, then run:

```bash
cmake --build build --target circuitstomp_protocol_test --parallel
```

Expected: build failure because the protocol header/function is missing.

- [ ] **Step 3: Implement the Prototype 1 decoder**

In the header, define named constants for MIDI 1.0 control-change status mask/value, channel 1, P1 CC 20, and the two relative no-op values. Implement only the P1 mapping. Convert values 1-63 directly and values 65-127 with `static_cast<int>(value) - 128`. Reject any data byte above 127.

- [ ] **Step 4: Verify GREEN and regressions**

Run both portable controller tests and the full CTest suite. Expected: zero failures.

- [ ] **Step 5: Commit the decoder**

```bash
git add CMakeLists.txt src/CircuitStompProtocol.h src/CircuitStompProtocol.cpp src/circuitstomp_protocol_test.cpp
git commit -m "feat: decode CircuitStomp prototype MIDI"
```

---

### Task 3: macOS authoritative target adapter

**Files:**
- Create: `src/MacControllerTarget.h`
- Create: `src/MacControllerTarget.cpp`
- Create: `src/mac_controller_target_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ControllerTarget` and `MacAudioEngine`.
- Produces: `MacControllerTarget(MacAudioEngine&)` implementing every target getter/setter without storing duplicate rig state.

- [ ] **Step 1: Write the macOS-only failing adapter test**

Create a test that instantiates `MacAudioEngine` without starting audio and verifies:

```cpp
circuitpedal::MacControllerTarget target(engine);
expect(target.parameterCount() == 2U, "built-in pedal exposes two contextual controls");
expect(target.parameterState(0).label == "Distortion", "built-in P1 label");
expect(target.parameterState(1).label == "Output", "built-in P2 label");
target.setParameterNormalized(0U, 0.72f);
expectNear(engine.distortion(), 0.72f, "built-in P1 writes engine distortion");
target.setMasterOutputNormalized(1.0f);
expectNear(engine.masterOutputDb(), 6.0f, "normalized master maps to +6 dB");
```

Load `circuits/woolly_mammoth_reference_draft.cpedal`, verify the adapter exposes the declared control count/order, verify a control write reaches `engine.circuitControl(index)`, and verify unavailable indices fail safely.

- [ ] **Step 2: Register and verify RED on macOS CI/build environment**

Inside `if(APPLE)`, add `MacControllerTarget.cpp` to a `circuit_mac_control` library linked to `circuit_control` and `circuit_mac_audio`, then register the test with `CIRCUITPEDAL_SOURCE_DIR`. Build it on macOS and confirm the missing adapter fails compilation before implementation.

- [ ] **Step 3: Implement direct engine adaptation**

For generic circuits, use `circuitControls()`, `circuitControl()`, and `setCircuitControl()`. For the built-in pedal, expose Distortion and Output using existing engine getters/setters. Use 0.01 steps for potentiometers; for switch controls return `1.0f / float(max(2U, positionCount) - 1U)`. Convert MASTER using:

```cpp
constexpr float minimumMasterDb = -18.0f;
constexpr float maximumMasterDb = 6.0f;
normalized = (engine.masterOutputDb() - minimumMasterDb)
           / (maximumMasterDb - minimumMasterDb);
db = minimumMasterDb
   + normalized * (maximumMasterDb - minimumMasterDb);
```

Delegate bypass directly to engine getters/setters.

- [ ] **Step 4: Verify the adapter on macOS**

Run `mac_controller_target_test` and full CTest on GitHub macOS CI. Expected: zero failures.

- [ ] **Step 5: Commit the adapter**

```bash
git add CMakeLists.txt src/MacControllerTarget.h src/MacControllerTarget.cpp src/mac_controller_target_test.cpp
git commit -m "feat: adapt mac audio engine to controller target"
```

---

### Task 4: CoreMIDI input adapter

**Files:**
- Create: `src/MacMidiInput.h`
- Create: `src/MacMidiInput.mm`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `circuitstomp::decodeMidi1Message(...)`.
- Produces: `MacMidiInput::start(ActionCallback, DiagnosticCallback)`, `stop()`, `running()`, and `connectedSourceCount()`.
- Guarantees: callbacks supplied to the GUI execute on the main queue; CoreMIDI callbacks never invoke AppKit, engine, or router code.

- [ ] **Step 1: Add the macOS target before implementation and verify RED**

Create the public header with move/copy disabled and an opaque `Impl`. Add `MacMidiInput.mm` to `circuit_mac_control`, link `-framework CoreMIDI`, and build on macOS. Expected: link failure until lifecycle methods are implemented.

- [ ] **Step 2: Implement client, port, and source reconciliation**

Use `MIDIClientCreate`, `MIDIInputPortCreate`, `MIDIGetNumberOfSources`, `MIDIGetSource`, `MIDIPortConnectSource`, and `MIDIPortDisconnectSource`. Track connected `MIDIEndpointRef` values. On CoreMIDI setup-change notifications, enqueue one reconciliation on the main queue. Keep successful source connections when another endpoint fails and report diagnostic text through the supplied callback.

- [ ] **Step 3: Implement bounded packet decoding and safe dispatch**

Walk every `MIDIPacket` in the list and scan for complete three-byte Control Change messages. Pass status/data bytes to the portable decoder. Copy each returned `ControllerAction` into a main-queue block. The block must lock/capture shared callback state, check its atomic `active` flag, then call the action callback. `stop()` first marks the state inactive, then disposes the port/client and clears endpoints/callbacks.

- [ ] **Step 4: Build the native targets**

On macOS run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Expected: CoreMIDI adapter and existing CLI/GUI targets build without warnings or errors.

- [ ] **Step 5: Commit CoreMIDI isolation**

```bash
git add CMakeLists.txt src/MacMidiInput.h src/MacMidiInput.mm
git commit -m "feat: add macOS CoreMIDI input adapter"
```

---

### Task 5: Compose routing and synchronize AppKit controls

**Files:**
- Modify: `src/main_mac_gui.mm`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `MacControllerTarget`, `ControllerRouter`, and `MacMidiInput`.
- Produces: one application-lifetime control path and `-refreshControllerStateFromEngine` UI synchronization.

- [ ] **Step 1: Add integration ownership and observe the initial build failure**

Add unique pointers after `_engine` in this destruction-safe order:

```cpp
std::unique_ptr<circuitpedal::MacControllerTarget> _controllerTarget;
std::unique_ptr<circuitpedal::ControllerRouter> _controllerRouter;
std::unique_ptr<circuitpedal::MacMidiInput> _midiInput;
```

Construct them after the engine and attempt the native build before adding callbacks/refresh code. Expected: build failure for incomplete composition or missing refresh selector.

- [ ] **Step 2: Route main-queue actions and expose non-fatal diagnostics**

Start MIDI with an action callback that routes the already-decoded `ControllerAction`. When `result.stateChanged` is true, call `refreshControllerStateFromEngine`. The diagnostic callback may update `_errorLabel` only because `MacMidiInput` guarantees main-queue delivery. MIDI startup failure must leave the application usable and must not block audio startup.

- [ ] **Step 3: Refresh UI from authoritative engine getters**

Implement `refreshControllerStateFromEngine` to update built-in Distortion/Output slider values, every visible generic slider/switch value, MASTER slider/value text, bypass button state, and bypass appearance from engine getters. Do not mutate engine state from this refresh method. Reuse this method after GUI-originated control changes where it removes duplicate formatting logic, but do not redesign the GUI.

- [ ] **Step 4: Stop MIDI before engine teardown**

In `applicationWillTerminate:`, call `_midiInput->stop()` before stopping the engine. Reset ownership only if necessary; declared reverse destruction order already destroys MIDI before router, target, and engine.

- [ ] **Step 5: Build and run full macOS tests**

Run the Release native build and full CTest suite on macOS. Expected: the bundle executable exists and all tests pass.

- [ ] **Step 6: Commit the vertical slice**

```bash
git add CMakeLists.txt src/main_mac_gui.mm
git commit -m "feat: route CircuitStomp controls into macOS host"
```

---

### Task 6: Protocol and architecture documentation

**Files:**
- Create: `docs/circuitstomp_midi_protocol.md`
- Modify: `docs/branch_status.md`

**Interfaces:**
- Documents: Prototype 1 wire mapping, reserved namespaces, authority model, and future feedback direction.

- [ ] **Step 1: Write the protocol document**

Document channel 1 CC 20, the `1..63` positive / `65..127` negative / `0,64` no-op convention, and the exact P1 byte examples. Mark the mapping provisional. Reserve—but do not assign as implemented—sections for P1-P5, MASTER, SW1-SW3, bypass, scenes/presets, EXP, pedal/parameter feedback, tuner, and connection/battery metadata. State that BLE MIDI and USB MIDI share the same MIDI 1.0 mapping and that CircuitPedal owns all rig state.

- [ ] **Step 2: Update branch status accurately**

Add `feature/circuitstomp-control-layer` to the active branch table as the unmerged controller-routing/CoreMIDI milestone. Do not describe it as accepted or merged.

- [ ] **Step 3: Check documentation and commit**

Run `git diff --check`, scan for `TBD`, `TODO`, and accidental claims that reserved mappings are implemented, then commit:

```bash
git add docs/circuitstomp_midi_protocol.md docs/branch_status.md
git commit -m "docs: define provisional CircuitStomp MIDI protocol"
```

---

### Task 7: Final verification, draft PR, and unmerged handoff

**Files:**
- Verify all changed files; make no unrelated edits.

**Interfaces:**
- Produces: pushed `feature/circuitstomp-control-layer` branch and an unmerged draft PR used to trigger Linux, sanitizer, and macOS CI.

- [ ] **Step 1: Run fresh local verification**

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
git diff --check main...HEAD
```

Expected on Linux: every portable/existing target builds and all registered tests pass.

- [ ] **Step 2: Inspect scope and forbidden dependencies**

Run:

```bash
git status --short --branch
git diff --stat main...HEAD
git diff --name-only main...HEAD
rg -n "CoreMIDI|AppKit|MacAudioEngine" src/ControllerAction.h src/ControllerTarget.h src/ControllerRouter.* src/CircuitStompProtocol.* src/controller_router_test.cpp src/circuitstomp_protocol_test.cpp
```

Expected: clean worktree; only planned files changed; the final search returns no matches.

- [ ] **Step 3: Push without merging**

```bash
git push -u origin feature/circuitstomp-control-layer
```

- [ ] **Step 4: Open a draft PR solely for CI verification**

Open a draft pull request from `feature/circuitstomp-control-layer` to `main`, state that physical CircuitStomp hardware has not yet been tested, and do not enable auto-merge.

- [ ] **Step 5: Verify all GitHub jobs**

Wait for and inspect the Ubuntu, macOS, sanitizer, and ngspice jobs. Confirm the macOS job builds `CircuitPedalGUI.app` and executes `mac_controller_target_test`. If any job fails, reproduce or inspect its logs, add a failing regression/compile test where possible, fix on the feature branch, rerun local verification, push, and wait for fresh CI.

- [ ] **Step 6: Report exact limitations**

Report commit/branch/PR links, test counts, native build evidence, implemented flow, and the remaining physical limitation: no CircuitStomp hardware is available to verify BLE pairing, USB enumeration, encoder feel/rate, or end-to-end host-to-controller feedback.

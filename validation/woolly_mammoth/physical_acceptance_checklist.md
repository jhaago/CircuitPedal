# Woolly Mammoth physical acceptance checklist

Use this checklist for the **48 kHz / 1x live path**. The 4x oversampling path is deferred and is not part of Woolly promotion.

Do not mark the model physically validated from a single favourable listening impression. Record the actual setup and exercise the full control range.

## Test record

- Date:
- Tester:
- Mac / host:
- Audio interface:
- Host sample rate: 48 kHz
- Buffer size:
- Guitar / bass:
- Pickup type:
- Monitoring chain:
- Reference pedal/build (if available):
- Reference pedal supply voltage:
- CircuitPedal commit:

## Basic live behaviour

- [ ] App starts in 1x processing mode.
- [ ] Audio passes reliably with no dropouts or hangs.
- [ ] No host solve failures during the test.
- [ ] No failed sub-solves during the test.
- [ ] No unexpected level jumps when loading the Woolly model.
- [ ] Bypass / dry comparison behaves as expected for the current test host.

## Control sweep

For each control, sweep slowly from minimum to maximum while playing both sustained notes and short/transient notes.

### WOOL

- [ ] Full range responds continuously.
- [ ] Minimum position is usable and does not produce a numerical fault.
- [ ] Maximum position is usable and does not produce a numerical fault.
- [ ] Direction of control matches the expected pedal behaviour.
- Notes:

### PINCH

- [ ] Full range responds continuously.
- [ ] Minimum position is usable and does not produce a numerical fault.
- [ ] Maximum position is usable and does not produce a numerical fault.
- [ ] Direction of control matches the expected pedal behaviour.
- Notes:

### EQ

- [ ] Full range responds continuously.
- [ ] Low and high extremes are distinct and musically plausible.
- [ ] Direction of control matches the expected pedal behaviour.
- Notes:

### OUTPUT

- [ ] Full range responds continuously.
- [ ] Minimum approaches mute as expected.
- [ ] Maximum does not cause an unexplained discontinuity or numerical fault.
- [ ] Direction of control matches the expected pedal behaviour.
- Notes:

## Combination / edge-state checks

- [ ] All controls at maximum.
- [ ] WOOL maximum / PINCH minimum.
- [ ] WOOL minimum / PINCH maximum.
- [ ] Low-WOOL restrained setting used by the automated campaign.
- [ ] Typical musical setting used for at least one minute of continuous playing.
- [ ] Hard picking / high input transient test.
- [ ] Sustained low notes.
- [ ] Sustained high notes.

## Subjective/reference comparison

If a physical Woolly Mammoth or trusted working clone is available, level-match before comparing.

- [ ] Overall fuzz character is recognisably in-family.
- [ ] Low-frequency response is comparable.
- [ ] Gating / compression behaviour is comparable.
- [ ] WOOL response is comparable.
- [ ] PINCH response is comparable.
- [ ] EQ sweep is comparable.
- [ ] Output level range is comparable.
- [ ] No obvious behaviour exists in CircuitPedal that is absent from the reference.
- [ ] No major reference behaviour is missing from CircuitPedal.

Reference-comparison notes:

## Acceptance decision

Choose one after completing the evidence above:

- [ ] **Reject / needs model changes**
- [ ] **Live-use accepted, fidelity still Reference Draft**
- [ ] **Physically validated model candidate** — only use this when the electrical and reference evidence also supports promotion

Decision notes:

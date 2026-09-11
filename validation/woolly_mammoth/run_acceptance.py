#!/usr/bin/env python3
"""Build a repeatable Woolly Mammoth acceptance report.

This runner intentionally separates three questions:

1. Is the current 48 kHz / 1x production path numerically safe across useful
   Woolly control states?
2. How close is the model's DC operating point to the published working-board
   reference?
3. Are the published DC numbers themselves internally representative enough to
   justify parameter fitting?

The first is a hard automated gate. The second reports both the historical 35%
saneness gate and the published ~10% working-board target, but only the 35% gate
is currently enforced in CI. The third is a reference-sanity calculation rather
than a pass/fail gate. Component-fidelity promotion still requires better
provenance / physical reference evidence.
"""

from __future__ import annotations

import argparse
import csv
import math
import re
import subprocess
import sys
from pathlib import Path


NODE_LINE = re.compile(r"^\s*([A-Za-z0-9_]+)\s*=\s*([-+0-9.eE]+)\s+V\s*$")

LIVE_CASES = (
    ("nominal", 0.75, 0.35, 0.50, 0.70),
    ("all_maximum", 1.00, 1.00, 1.00, 1.00),
    ("restrained", 0.25, 0.70, 0.25, 0.70),
    ("wool_high_pinch_low", 1.00, 0.00, 0.50, 0.70),
    ("wool_low_pinch_high", 0.00, 1.00, 0.50, 0.70),
)


def run(command: list[str], *, allow_failure: bool = False) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode != 0 and not allow_failure:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(command)}")
    return result


def parse_dc_output(text: str) -> dict[str, float]:
    values: dict[str, float] = {}
    for raw in text.splitlines():
        match = NODE_LINE.match(raw)
        if match:
            values[match.group(1)] = float(match.group(2))
    return values


def load_reference(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def summarize_render(path: Path) -> dict[str, float]:
    outputs: list[float] = []
    convergence_failures = 0
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            value = float(row["output_fs"])
            if not math.isfinite(value):
                raise RuntimeError(f"non-finite output in {path}")
            outputs.append(value)
            if row.get("converged") == "0":
                convergence_failures += 1

    if not outputs:
        raise RuntimeError(f"render contained no samples: {path}")

    peak = max(abs(value) for value in outputs)
    rms = math.sqrt(sum(value * value for value in outputs) / len(outputs))
    clipped = sum(1 for value in outputs if abs(value) >= 0.999999)
    return {
        "samples": float(len(outputs)),
        "peak_fs": peak,
        "rms_fs": rms,
        "clip_percent": 100.0 * clipped / len(outputs),
        "convergence_failures": float(convergence_failures),
    }


def implied_betas(reference: dict[str, float], supply: float, feedback_ohms: float) -> tuple[float, float]:
    """Infer effective DC current gains from the published node voltages.

    At DC the coupling/bypass capacitors are open. The feedback path from Q2
    emitter to Q1 base is therefore the only Q1-base current path represented by
    this compact Woolly topology. `feedback_ohms` includes the fixed 100k plus
    whichever end-state of the 500k PINCH rheostat is being considered.
    """

    b1 = reference["B1"]
    q2_base = reference["C1_NODE"]
    e2 = reference["E2"]
    c2 = reference["C2_NODE"]

    ib1 = (e2 - b1) / feedback_ohms
    r3_current = (supply - q2_base) / 51_000.0
    ic2 = (supply - c2) / 20_000.0
    emitter_resistor_current = e2 / 2_200.0
    ie2 = emitter_resistor_current + ib1
    ib2 = ie2 - ic2
    ic1 = r3_current - ib2

    if ib1 <= 0.0 or ib2 <= 0.0 or ic1 <= 0.0 or ic2 <= 0.0:
        raise RuntimeError("published DC reference produces non-positive inferred BJT currents")
    return ic1 / ib1, ic2 / ib2


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("validator", help="path to circuitpedal_validate")
    parser.add_argument(
        "output_dir",
        nargs="?",
        default="build/validation/woolly-acceptance",
        help="directory for report and render CSVs",
    )
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    repo = script_dir.parent.parent
    validator = Path(args.validator).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    circuit = repo / "circuits" / "woolly_mammoth_reference_draft.cpedal"
    reference_csv = script_dir / "working_board_dc.csv"
    report_path = output_dir / "acceptance_report.md"

    if not validator.is_file():
        print(f"validator not found: {validator}", file=sys.stderr)
        return 2

    report: list[str] = [
        "# Woolly Mammoth acceptance report",
        "",
        "This report evaluates the current **48 kHz / 1x production path** and",
        "the published working-board DC reference. It does not test or promote",
        "the deferred 4x oversampling path.",
        "",
        "## 1. Published working-board DC comparison",
        "",
    ]

    dc_command = [
        str(validator),
        "dc",
        str(circuit),
        "--sample-rate", "48000",
        "--source", "VCCSRC=9.33",
        "--control", "WOOL=1",
        "--control", "PINCH=1",
        "--control", "EQ=1",
        "--control", "OUTPUT=1",
        "--node", "B1",
        "--node", "C1_NODE",
        "--node", "E2",
        "--node", "C2_NODE",
    ]
    dc_result = run(dc_command)
    actual = parse_dc_output(dc_result.stdout)
    references = load_reference(reference_csv)

    report.extend([
        "| Node | Reference (V) | CircuitPedal (V) | Error (V) | Error (%) | ±10% target | Legacy 35% |",
        "| --- | ---: | ---: | ---: | ---: | :---: | :---: |",
    ])

    ten_percent_pass = True
    legacy_pass = True
    reference_values: dict[str, float] = {}
    for row in references:
        node = row["node"]
        expected = float(row["expected_v"])
        reference_values[node] = expected
        if node not in actual:
            raise RuntimeError(f"DC output did not contain reference node {node}")
        measured = actual[node]
        difference = measured - expected
        relative_percent = (abs(difference) / abs(expected) * 100.0) if expected else 0.0
        point_10 = relative_percent <= 10.0
        point_35 = relative_percent <= 35.0
        ten_percent_pass = ten_percent_pass and point_10
        legacy_pass = legacy_pass and point_35
        report.append(
            f"| {node} | {expected:.4f} | {measured:.4f} | {difference:+.4f} | "
            f"{relative_percent:.2f}% | {'PASS' if point_10 else 'FAIL'} | "
            f"{'PASS' if point_35 else 'FAIL'} |"
        )

    supply = float(references[0]["source_supply_v"])
    beta1_current, beta2_current = implied_betas(reference_values, supply, 600_000.0)
    beta1_alternate, beta2_alternate = implied_betas(reference_values, supply, 100_000.0)

    report.extend([
        "",
        f"**Published ~±10% working-board target:** {'PASS' if ten_percent_pass else 'NOT YET MET'}",
        "",
        f"**Historical 35% sanity gate:** {'PASS' if legacy_pass else 'FAIL'}",
        "",
        "The ±10% result is reported as evidence, not silently weakened. It is not",
        "yet a CI failure because the published voltages are from another working",
        "build/transistor lot and the current compact transistor model does not have",
        "manufacturer-model provenance.",
        "",
        "### Reference self-consistency sanity check",
        "",
        "Using the published voltages with the 51k collector feed, 20k Q2 collector",
        "resistor, 2.2k emitter resistor and the feedback path gives the following",
        "effective DC current gains. This is not transistor parameter fitting; it is",
        "a KCL sanity check on what the external voltage set implies.",
        "",
        "| PINCH end-state assumption | Implied Q1 β | Implied Q2 β |",
        "| --- | ---: | ---: |",
        f"| 100k fixed + 500k rheostat (current all-max mapping) | {beta1_current:.1f} | {beta2_current:.1f} |",
        f"| 100k fixed + ~0Ω rheostat (opposite mechanical-end assumption) | {beta1_alternate:.1f} | {beta2_alternate:.1f} |",
        "",
        "The Q2 value remains around 7 under either PINCH-end interpretation. That is",
        "a strong warning against treating this one working-board voltage set as a",
        "golden component-calibration target. It can still serve as a broad operating-",
        "region check while a nominated physical reference is obtained.",
        "",
        "## 2. 48 kHz / 1x live-path solver sweep",
        "",
        "Each state renders a 110 Hz sine input at 0.25 FS after a short warm-up.",
        "Any nonlinear convergence failure or non-finite output fails this acceptance run.",
        "",
        "| State | Peak (FS) | RMS (FS) | Clipped samples | Convergence failures |",
        "| --- | ---: | ---: | ---: | ---: |",
    ])

    live_pass = True
    for name, wool, pinch, eq, output in LIVE_CASES:
        csv_path = output_dir / f"live_{name}.csv"
        command = [
            str(validator), "render", str(circuit), str(csv_path),
            "--sample-rate", "48000",
            "--seconds", "0.25",
            "--warmup", "0.05",
            "--signal", "sine",
            "--frequency", "110",
            "--amplitude", "0.25",
            "--control", f"WOOL={wool}",
            "--control", f"PINCH={pinch}",
            "--control", f"EQ={eq}",
            "--control", f"OUTPUT={output}",
            "--node", "B1",
            "--node", "C1_NODE",
            "--node", "E2",
            "--node", "C2_NODE",
        ]
        result = run(command, allow_failure=True)
        if result.returncode != 0:
            live_pass = False
            report.append(f"| {name} | — | — | — | command failed ({result.returncode}) |")
            continue
        metrics = summarize_render(csv_path)
        if metrics["convergence_failures"] != 0.0:
            live_pass = False
        report.append(
            f"| {name} | {metrics['peak_fs']:.6f} | {metrics['rms_fs']:.6f} | "
            f"{metrics['clip_percent']:.3f}% | {int(metrics['convergence_failures'])} |"
        )

    report.extend([
        "",
        f"**48 kHz / 1x numerical safety gate:** {'PASS' if live_pass else 'FAIL'}",
        "",
        "## 3. Promotion status",
        "",
        "Passing this report means the current live path is numerically suitable for",
        "continued physical evaluation. It does **not** by itself promote the pedal",
        "from Reference Draft to a component-accurate model.",
        "",
        "Remaining fidelity evidence before promotion:",
        "",
        "- obtain a nominated physical Woolly/verified clone and record its own DC voltages;",
        "- corroborate the 2N3904 model with a versioned manufacturer SPICE model or",
        "  measurements from a nominated transistor lot;",
        "- compare transient node/output waveforms across multiple control states;",
        "- complete the manual physical control-range/listening checklist;",
        "- record the reference hardware, supply voltage, audio interface and test date.",
        "",
    ])

    report_path.write_text("\n".join(report), encoding="utf-8")
    print(report_path.read_text(encoding="utf-8"))
    print(f"Report written to: {report_path}")

    # The live-path safety gate and existing legacy DC sanity limit are hard
    # failures. The tighter ~10% fidelity target remains visible but non-gating
    # until the reference/model provenance is strong enough to justify it.
    return 0 if live_pass and legacy_pass else 3


if __name__ == "__main__":
    raise SystemExit(main())

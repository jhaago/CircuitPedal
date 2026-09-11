#!/usr/bin/env python3
"""Compare Woolly DC bias against physical and fuller ngspice transistor models.

This is a diagnostic report, not a promotion gate. It asks whether the current
compact Ebers-Moll 2N3904 approximation or the circuit topology is the more
likely source of the published working-board DC disagreement.
"""

from __future__ import annotations

import argparse
import csv
import re
import shutil
import subprocess
import sys
from pathlib import Path


NODE_LINE = re.compile(r"^\s*([A-Za-z0-9_]+)\s*=\s*([-+0-9.eE]+)\s+V\s*$")
NODES = ("B1", "C1_NODE", "E2", "C2_NODE")


def parse_circuitpedal_dc(text: str) -> dict[str, float]:
    values: dict[str, float] = {}
    for raw in text.splitlines():
        match = NODE_LINE.match(raw)
        if match:
            values[match.group(1)] = float(match.group(2))
    return values


def build_op_netlist(source: Path, destination: Path) -> None:
    source_lines = source.read_text(encoding="utf-8").splitlines()
    output: list[str] = []
    in_control = False
    for line in source_lines:
        stripped = line.strip().lower()
        if stripped == ".control":
            in_control = True
            output.extend([
                ".control",
                "set wr_singlescale",
                "op",
                "wrdata woolly_mammoth_op.dat v(B1) v(C1_NODE) v(E2) v(C2_NODE)",
                "quit",
                ".endc",
            ])
            continue
        if in_control:
            if stripped == ".endc":
                in_control = False
            continue

        if line.startswith(".param SUPPLY="):
            line = ".param SUPPLY=9.33"
        elif line.startswith(".param INPUT_FS="):
            line = ".param INPUT_FS=0"
        elif line.startswith(".param WOOL="):
            line = ".param WOOL=1"
        elif line.startswith(".param PINCH="):
            line = ".param PINCH=1"
        elif line.startswith(".param EQPOS="):
            line = ".param EQPOS=1"
        elif line.startswith(".param OUTPUT_POS="):
            line = ".param OUTPUT_POS=1"
        output.append(line)

    destination.write_text("\n".join(output) + "\n", encoding="utf-8")


def parse_ngspice_wrdata(path: Path) -> dict[str, float]:
    rows = [line.split() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not rows:
        raise RuntimeError("ngspice OP wrdata output was empty")
    values = [float(value) for value in rows[-1]]
    # With `set wr_singlescale`, wrdata writes one scale column followed by one
    # column per requested vector. Keep a fallback for builds that omit scale.
    if len(values) == len(NODES) + 1:
        values = values[1:]
    elif len(values) != len(NODES):
        raise RuntimeError(
            f"unexpected ngspice OP column count {len(values)} in {path}: {rows[-1]}"
        )
    return dict(zip(NODES, values))


def percent_error(actual: float, expected: float) -> float:
    return abs(actual - expected) / abs(expected) * 100.0 if expected else 0.0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("validator", help="path to circuitpedal_validate")
    parser.add_argument(
        "output_dir",
        nargs="?",
        default="build/validation/woolly-dc-model-comparison",
    )
    args = parser.parse_args()

    ngspice = shutil.which("ngspice")
    if ngspice is None:
        print("ngspice is required", file=sys.stderr)
        return 2

    script_dir = Path(__file__).resolve().parent
    repo = script_dir.parent.parent
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    validator = Path(args.validator).resolve()
    circuit = repo / "circuits" / "woolly_mammoth_reference_draft.cpedal"
    source_netlist = script_dir / "woolly_mammoth_ngspice.cir"
    effective_netlist = output_dir / "woolly_mammoth_op_effective.cir"
    build_op_netlist(source_netlist, effective_netlist)

    ng_result = subprocess.run(
        [ngspice, "-b", "-o", "woolly_mammoth_op.log", effective_netlist.name],
        cwd=output_dir,
        text=True,
        capture_output=True,
        check=False,
    )
    if ng_result.returncode != 0:
        sys.stderr.write(ng_result.stdout)
        sys.stderr.write(ng_result.stderr)
        log = output_dir / "woolly_mammoth_op.log"
        if log.exists():
            sys.stderr.write(log.read_text(encoding="utf-8", errors="replace"))
        return ng_result.returncode

    ng_values = parse_ngspice_wrdata(output_dir / "woolly_mammoth_op.dat")

    cp_command = [
        str(validator), "dc", str(circuit),
        "--sample-rate", "48000",
        "--source", "VCCSRC=9.33",
        "--control", "WOOL=1",
        "--control", "PINCH=1",
        "--control", "EQ=1",
        "--control", "OUTPUT=1",
    ]
    for node in NODES:
        cp_command.extend(["--node", node])
    cp_result = subprocess.run(cp_command, text=True, capture_output=True, check=False)
    if cp_result.returncode != 0:
        sys.stderr.write(cp_result.stdout)
        sys.stderr.write(cp_result.stderr)
        return cp_result.returncode
    cp_values = parse_circuitpedal_dc(cp_result.stdout)

    with (script_dir / "working_board_dc.csv").open(newline="", encoding="utf-8") as handle:
        reference = {row["node"]: float(row["expected_v"]) for row in csv.DictReader(handle)}

    report = [
        "# Woolly Mammoth DC transistor-model triage",
        "",
        "Condition: 9.33 V supply, no audio input, all four controls at maximum.",
        "",
        "The ngspice column uses the existing fuller Gummel-Poon Q2N3904 candidate.",
        "It is diagnostic evidence only; that model still lacks nominated manufacturer",
        "provenance in this repository and is not a golden reference.",
        "",
        "| Node | Published board (V) | CircuitPedal compact 2N3904 (V) | CP error | ngspice Gummel-Poon (V) | ngspice error |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]

    cp_total = 0.0
    ng_total = 0.0
    for node in NODES:
        expected = reference[node]
        cp = cp_values[node]
        ng = ng_values[node]
        cp_error = percent_error(cp, expected)
        ng_error = percent_error(ng, expected)
        cp_total += cp_error
        ng_total += ng_error
        report.append(
            f"| {node} | {expected:.4f} | {cp:.4f} | {cp_error:.2f}% | "
            f"{ng:.4f} | {ng_error:.2f}% |"
        )

    cp_mean = cp_total / len(NODES)
    ng_mean = ng_total / len(NODES)
    report.extend([
        "",
        f"Mean absolute percentage error: CircuitPedal **{cp_mean:.2f}%**, "
        f"ngspice candidate **{ng_mean:.2f}%**.",
        "",
    ])

    if ng_mean + 1.0 < cp_mean:
        report.extend([
            "**Diagnostic indication:** the fuller transistor model moves the DC bias",
            "meaningfully closer to the published board overall. Prioritise transistor-model",
            "fidelity/provenance before changing the Woolly topology or resistor values.",
        ])
    elif cp_mean + 1.0 < ng_mean:
        report.extend([
            "**Diagnostic indication:** the fuller candidate transistor model does not improve",
            "overall agreement. Do not assume the compact transistor model is the main cause;",
            "re-check topology, component tolerances and the physical reference conditions.",
        ])
    else:
        report.extend([
            "**Diagnostic indication:** the two transistor models have similar overall DC error.",
            "The present evidence does not isolate transistor modelling as the dominant cause.",
        ])

    report.extend([
        "",
        "Do not tune circuit values solely to minimise this four-point table. Physical device",
        "spread, resistor tolerance and the provenance of both reference datasets must be",
        "accounted for before model parameters become production constants.",
        "",
    ])

    report_path = output_dir / "dc_model_comparison.md"
    report_path.write_text("\n".join(report), encoding="utf-8")
    print(report_path.read_text(encoding="utf-8"))
    print(f"Report written to: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

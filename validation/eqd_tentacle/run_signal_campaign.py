#!/usr/bin/env python3
"""Render EQD Tentacle at realistic guitar-scale inputs and summarize each stage.

The Tentacle circuit declares AUDIO scale 0.20 V per digital full scale. The
campaign therefore uses digital amplitudes 0.10, 0.25 and 0.50, corresponding
to approximately 20 mVpk, 50 mVpk and 100 mVpk at the circuit input.

A 440 Hz sine is used to match published Green Ringer analysis and to make the
expected 880 Hz octave component easy to inspect.
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


AMPLITUDES = [
    ("20mV", 0.10),
    ("50mV", 0.25),
    ("100mV", 0.50),
]

NODES = [
    "Q1_BASE",
    "Q1_COLLECTOR",
    "Q1_EMITTER",
    "Q2_EMITTER",
    "Q2_COLLECTOR",
    "RECT_UPPER",
    "RECT_LOWER",
    "Q3_BASE",
    "Q3_EMITTER",
    "OUT",
]


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("validator")
    parser.add_argument("output_dir")
    args = parser.parse_args()

    root = pathlib.Path(__file__).resolve().parents[2]
    circuit = root / "circuits" / "eqd_tentacle_reference_draft.cpedal"
    summary = root / "validation" / "model_signal_summary.py"
    output_dir = pathlib.Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    report_path = output_dir / "tentacle_signal_report.txt"
    with report_path.open("w", encoding="utf-8") as report:
        for label, amplitude in AMPLITUDES:
            csv_path = output_dir / f"tentacle_{label}.csv"
            command = [
                args.validator,
                "render",
                str(circuit),
                str(csv_path),
                "--sample-rate", "48000",
                "--seconds", "0.12",
                "--warmup", "0.02",
                "--signal", "sine",
                "--frequency", "440",
                "--amplitude", str(amplitude),
            ]
            for node in NODES:
                command += ["--node", node]
            run(command)

            result = subprocess.run(
                [
                    sys.executable,
                    str(summary),
                    str(csv_path),
                    "--fundamental", "440",
                    "--label", f"Tentacle {label} input",
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            print(result.stdout, end="")
            report.write(result.stdout)
            report.write("\n")

    print(f"Wrote {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

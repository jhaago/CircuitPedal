#!/usr/bin/env python3
"""Validate Tentacle level, phase splitting, and octave generation.

The model maps digital full scale to 0.20 V at its input. Cases below cover
20-100 mV peak circuit inputs and low-to-high guitar fundamentals. The checks
exercise the real .cpedal through the offline renderer rather than duplicating
its circuit equations in Python.
"""

from __future__ import annotations

import argparse
import csv
import math
import pathlib
import subprocess


CASES = [
    ("low_E", 82.41, 0.10, False),
    ("A2", 110.00, 0.25, True),
    ("A4_low", 440.00, 0.10, False),
    ("A4", 440.00, 0.25, True),
    ("A5", 880.00, 0.25, True),
    ("E6", 1318.51, 0.50, True),
]

NODES = [
    "Q1_COLLECTOR",
    "Q2_EMITTER",
    "Q2_COLLECTOR",
    "RECT_UPPER",
    "RECT_LOWER",
    "Q3_BASE",
    "OUT",
]


def ac_rms(values: list[float]) -> float:
    mean = sum(values) / len(values)
    return math.sqrt(sum((value - mean) ** 2 for value in values) / len(values))


def correlation(left: list[float], right: list[float]) -> float:
    left_mean = sum(left) / len(left)
    right_mean = sum(right) / len(right)
    numerator = 0.0
    left_power = 0.0
    right_power = 0.0
    for left_raw, right_raw in zip(left, right):
        left_value = left_raw - left_mean
        right_value = right_raw - right_mean
        numerator += left_value * right_value
        left_power += left_value * left_value
        right_power += right_value * right_value
    denominator = math.sqrt(left_power * right_power)
    return numerator / denominator if denominator > 0.0 else 0.0


def projection(values: list[float], sample_rate: float, frequency: float) -> float:
    mean = sum(values) / len(values)
    sine = 0.0
    cosine = 0.0
    omega = 2.0 * math.pi * frequency / sample_rate
    for index, raw in enumerate(values):
        value = raw - mean
        phase = omega * index
        sine += value * math.sin(phase)
        cosine += value * math.cos(phase)
    return 2.0 * math.hypot(sine, cosine) / len(values)


def read_column(rows: list[dict[str, str]], name: str) -> list[float]:
    return [float(row[name]) for row in rows]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("validator")
    parser.add_argument("output_dir")
    args = parser.parse_args()

    root = pathlib.Path(__file__).resolve().parents[2]
    circuit = root / "circuits" / "eqd_tentacle_reference_draft.cpedal"
    output_dir = pathlib.Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    failures: list[str] = []
    report_lines = [
        "case,fundamental_hz,input_peak_mv,output_fs_rms,dry_rms,level_ratio,"
        "h2_over_h1,q2_phase_rms_ratio,q2_phase_correlation,convergence_failures"
    ]

    for label, frequency, amplitude, require_near_unity in CASES:
        csv_path = output_dir / f"tentacle_{label}.csv"
        command = [
            args.validator,
            "render",
            str(circuit),
            str(csv_path),
            "--sample-rate", "48000",
            "--seconds", "0.25",
            "--warmup", "0.05",
            "--signal", "sine",
            "--frequency", str(frequency),
            "--amplitude", str(amplitude),
        ]
        for node in NODES:
            command += ["--node", node]
        subprocess.run(command, check=True)

        with csv_path.open(newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))
        times = read_column(rows, "time_s")
        sample_rate = (len(times) - 1) / (times[-1] - times[0])
        output = read_column(rows, "output_fs")
        q2_emitter = read_column(rows, "node_Q2_EMITTER_v")
        q2_collector = read_column(rows, "node_Q2_COLLECTOR_v")

        output_rms = ac_rms(output)
        dry_rms = amplitude / math.sqrt(2.0)
        level_ratio = output_rms / dry_rms
        h1 = projection(output, sample_rate, frequency)
        h2 = projection(output, sample_rate, 2.0 * frequency)
        h2_over_h1 = h2 / max(h1, 1.0e-12)
        phase_ratio = ac_rms(q2_emitter) / ac_rms(q2_collector)
        phase_correlation = correlation(q2_emitter, q2_collector)
        convergence_failures = sum(row["converged"] == "0" for row in rows)

        report_lines.append(
            f"{label},{frequency:.2f},{amplitude * 200.0:.1f},"
            f"{output_rms:.8f},{dry_rms:.8f},{level_ratio:.4f},"
            f"{h2_over_h1:.2f},{phase_ratio:.4f},{phase_correlation:.5f},"
            f"{convergence_failures}"
        )

        if convergence_failures:
            failures.append(f"{label}: {convergence_failures} solver failures")
        if h2_over_h1 < 10.0:
            failures.append(f"{label}: octave ratio {h2_over_h1:.2f} is below 10")
        if not 0.90 <= phase_ratio <= 1.10:
            failures.append(f"{label}: Q2 phase RMS ratio {phase_ratio:.3f} is unbalanced")
        if phase_correlation > -0.95:
            failures.append(
                f"{label}: Q2 outputs are not sufficiently opposed "
                f"(correlation {phase_correlation:.4f})"
            )
        # Weak, low-register notes are deliberately allowed to gate by roughly
        # 9 dB; requiring unity here would tune out a defining analogue-octave
        # behaviour. Normal 50-100 mV inputs retain the tighter level guard.
        minimum_level = 0.80 if require_near_unity else 0.35
        if not minimum_level <= level_ratio <= 1.40:
            failures.append(
                f"{label}: output/dry RMS ratio {level_ratio:.3f} is outside "
                f"{minimum_level:.2f}-1.40"
            )

    report_path = output_dir / "tentacle_signal_report.csv"
    report_path.write_text("\n".join(report_lines) + "\n", encoding="utf-8")
    print(report_path.read_text(encoding="utf-8"), end="")
    for failure in failures:
        print(f"FAIL: {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())

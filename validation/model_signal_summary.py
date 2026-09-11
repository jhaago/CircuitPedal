#!/usr/bin/env python3
"""Summarize CircuitPedal validation-render CSVs for model debugging.

This is intentionally dependency-free so it can run in GitHub Actions.
For each numeric voltage/output column it reports DC mean, AC RMS, peak-to-peak,
and absolute peak. When a fundamental is supplied it also reports the first
five harmonic amplitudes using a direct sinusoidal projection.
"""

import argparse
import csv
import math
import sys


def projection_amplitude(values, sample_rate, frequency):
    n = len(values)
    if n == 0:
        return 0.0
    mean = sum(values) / n
    sin_sum = 0.0
    cos_sum = 0.0
    omega = 2.0 * math.pi * frequency / sample_rate
    for i, raw in enumerate(values):
        value = raw - mean
        phase = omega * i
        sin_sum += value * math.sin(phase)
        cos_sum += value * math.cos(phase)
    return 2.0 * math.hypot(sin_sum, cos_sum) / n


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path")
    parser.add_argument("--fundamental", type=float, default=0.0)
    parser.add_argument("--label", default="signal")
    args = parser.parse_args()

    with open(args.csv_path, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)

    if not rows:
        print(f"{args.label}: no rows")
        return 2

    try:
        times = [float(row["time_s"]) for row in rows]
    except (KeyError, ValueError):
        print("CSV is missing numeric time_s", file=sys.stderr)
        return 2

    if len(times) < 2:
        print("CSV is too short", file=sys.stderr)
        return 2

    dt = (times[-1] - times[0]) / (len(times) - 1)
    sample_rate = 1.0 / dt if dt > 0.0 else 0.0
    print(f"=== {args.label} ===")
    print(f"samples={len(rows)} sample_rate={sample_rate:.3f} Hz")

    skip = {"sample", "time_s", "input_fs", "converged"}
    columns = [name for name in (reader.fieldnames or []) if name not in skip]
    for name in columns:
        try:
            values = [float(row[name]) for row in rows]
        except (KeyError, ValueError):
            continue
        mean = sum(values) / len(values)
        centered = [value - mean for value in values]
        ac_rms = math.sqrt(sum(value * value for value in centered) / len(centered))
        minimum = min(values)
        maximum = max(values)
        absolute_peak = max(abs(value) for value in centered)
        print(
            f"{name:28s} mean={mean:+.6f}  ac_rms={ac_rms:.6f}  "
            f"p-p={maximum-minimum:.6f}  ac_peak={absolute_peak:.6f}"
        )

        if args.fundamental > 0.0 and name in {"output_v", "output_fs"}:
            harmonics = [
                projection_amplitude(values, sample_rate, args.fundamental * harmonic)
                for harmonic in range(1, 6)
            ]
            harmonic_text = " ".join(
                f"H{index + 1}={amplitude:.6g}"
                for index, amplitude in enumerate(harmonics)
            )
            print(f"  harmonics: {harmonic_text}")

    failed = sum(1 for row in rows if row.get("converged") == "0")
    print(f"convergence_failures={failed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

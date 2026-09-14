#!/usr/bin/env python3
"""Repeatable 1x validation campaign for the Human Gear Animato model.

The hard gates in this script are deliberately limited to behaviour that is
supported by the traced circuit or by numerical-safety requirements. It does
not try to tune the pedal by ear and it does not require bass preservation that
the original Rangemaster-fronted circuit does not provide.
"""

from __future__ import annotations

import argparse
import csv
import math
import subprocess
import sys
from pathlib import Path


TEST_SECONDS = 0.18
WARMUP_SECONDS = 0.06


def run(command: list[str], *, allow_failure: bool = False) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode != 0 and not allow_failure:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(command)}")
    return result


def read_column(path: Path, column: str) -> tuple[list[float], int]:
    values: list[float] = []
    convergence_failures = 0
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            if column not in row:
                raise RuntimeError(f"column {column!r} is missing from {path}")
            value = float(row[column])
            if not math.isfinite(value):
                raise RuntimeError(f"non-finite {column} in {path}")
            values.append(value)
            if row.get("converged") == "0":
                convergence_failures += 1
    if not values:
        raise RuntimeError(f"render contained no samples: {path}")
    return values, convergence_failures


def ac_rms(values: list[float]) -> float:
    mean = sum(values) / len(values)
    return math.sqrt(sum((value - mean) ** 2 for value in values) / len(values))


def rms(values: list[float]) -> float:
    return math.sqrt(sum(value * value for value in values) / len(values))


def harmonic_amplitudes(values: list[float], sample_rate: float, fundamental: float, count: int = 6) -> list[float]:
    """Return single-bin sinusoidal amplitudes after removing DC.

    Test durations are selected to contain many cycles; exact FFT-bin alignment
    is not required because these values are diagnostic ratios rather than a
    golden-reference spectral comparison.
    """
    mean = sum(values) / len(values)
    centered = [value - mean for value in values]
    amplitudes: list[float] = []
    n_samples = len(centered)
    for harmonic in range(1, count + 1):
        frequency = fundamental * harmonic
        real = 0.0
        imag = 0.0
        for index, value in enumerate(centered):
            phase = 2.0 * math.pi * frequency * index / sample_rate
            real += value * math.cos(phase)
            imag -= value * math.sin(phase)
        amplitudes.append(2.0 * math.hypot(real, imag) / n_samples)
    return amplitudes


def db(value: float, floor: float = -180.0) -> float:
    return 20.0 * math.log10(value) if value > 0.0 else floor


def summarize_output(path: Path, sample_rate: float, fundamental: float) -> dict[str, float]:
    values, failures = read_column(path, "output_fs")
    peak = max(abs(value) for value in values)
    output_rms = rms(values)
    clipped = sum(1 for value in values if abs(value) >= 0.999999)
    harmonics = harmonic_amplitudes(values, sample_rate, fundamental)
    fundamental_amp = harmonics[0]
    harmonic_power = sum(amplitude * amplitude for amplitude in harmonics[1:])
    thd = math.sqrt(harmonic_power) / fundamental_amp if fundamental_amp > 1.0e-15 else 0.0
    return {
        "peak_fs": peak,
        "rms_fs": output_rms,
        "clip_percent": 100.0 * clipped / len(values),
        "convergence_failures": float(failures),
        "fundamental": fundamental_amp,
        "thd_percent": 100.0 * thd,
        "h2_db": db(harmonics[1] / fundamental_amp) if fundamental_amp > 1.0e-15 else -180.0,
        "h3_db": db(harmonics[2] / fundamental_amp) if fundamental_amp > 1.0e-15 else -180.0,
    }


def render(
    validator: Path,
    circuit: Path,
    output: Path,
    *,
    sample_rate: float = 48000.0,
    frequency: float = 196.0,
    amplitude: float = 0.25,
    boost: float = 0.65,
    distortion: float = 0.65,
    tone: float = 0.50,
    volume: float = 0.55,
    bias: int = 0,
    nodes: tuple[str, ...] = (),
) -> subprocess.CompletedProcess[str]:
    command = [
        str(validator), "render", str(circuit), str(output),
        "--sample-rate", str(sample_rate),
        "--seconds", str(TEST_SECONDS),
        "--warmup", str(WARMUP_SECONDS),
        "--signal", "sine",
        "--frequency", str(frequency),
        "--amplitude", str(amplitude),
        "--control", f"BOOST={boost}",
        "--control", f"DISTORTION={distortion}",
        "--control", f"TONE={tone}",
        "--control", f"VOLUME={volume}",
        "--switch", f"BIAS={bias}",
    ]
    for node in nodes:
        command.extend(["--node", node])
    return run(command, allow_failure=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("validator", help="path to circuitpedal_validate")
    parser.add_argument(
        "output_dir",
        nargs="?",
        default="build/validation/animato-acceptance",
        help="directory for report and render CSV files",
    )
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    repo = script_dir.parent.parent
    validator = Path(args.validator).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    circuit = repo / "circuits" / "animato_reference_draft.cpedal"
    report_path = output_dir / "acceptance_report.md"

    if not validator.is_file():
        print(f"validator not found: {validator}", file=sys.stderr)
        return 2

    report: list[str] = [
        "# Human Gear Animato 1x acceptance report",
        "",
        "This campaign exercises the same component-level GenericCircuit solver used by",
        "the accepted live 1x path. It intentionally does not use the experimental 4x",
        "wrapper and does not add a clean blend or artificial bass preservation.",
        "",
        "## 1. Confirmed control-orientation gate",
        "",
    ]

    # The traced Rangemaster trimmer has one end at the AC-ground supply rail and
    # the other at the Sziklai output. CircuitPedal defines normalized position 0
    # at terminal 1 and position 1 at terminal 3. Therefore a user-facing higher
    # BOOST value must move the wiper toward the signal end, not toward VA.
    boost_rows: list[tuple[str, float, float, int]] = []
    boost_pass = True
    for name, position in (("minimum", 0.0), ("maximum", 1.0)):
        csv_path = output_dir / f"boost_{name}.csv"
        result = render(
            validator,
            circuit,
            csv_path,
            frequency=440.0,
            amplitude=0.05,
            boost=position,
            distortion=0.65,
            tone=0.50,
            volume=0.55,
            nodes=("BOOST_W",),
        )
        if result.returncode != 0:
            boost_pass = False
            boost_rows.append((name, position, 0.0, -1))
            continue
        node_values, failures = read_column(csv_path, "node_BOOST_W_v")
        boost_rows.append((name, position, ac_rms(node_values), failures))
        if failures:
            boost_pass = False

    if len(boost_rows) == 2:
        low_ac = boost_rows[0][2]
        high_ac = boost_rows[1][2]
        # Require a clear direction, not a tiny numerical difference.
        if not (high_ac > low_ac * 2.0 and high_ac > 1.0e-7):
            boost_pass = False

    report.extend([
        "The electrical signal at `BOOST_W` should increase as the exposed BOOST",
        "control moves from 0 to 1.",
        "",
        "| Position | Normalized | BOOST_W AC RMS (V) | Solver failures |",
        "| --- | ---: | ---: | ---: |",
    ])
    for name, position, level, failures in boost_rows:
        report.append(f"| {name} | {position:.2f} | {level:.9f} | {failures} |")
    report.extend([
        "",
        f"**BOOST direction gate:** {'PASS' if boost_pass else 'FAIL'}",
        "",
        "## 2. Gain / clipping baseline",
        "",
        "These rows are diagnostic except for numerical safety. Input amplitude 0.25 FS",
        "corresponds to 50 mV peak at the model's 0.20 V/FS input calibration.",
        "",
        "| State | Peak FS | RMS FS | Clip % | THD % | H2/fund dB | H3/fund dB | Solver failures |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ])

    safety_pass = True
    cases = (
        ("distortion_low", 196.0, 0.25, 0.65, 0.05, 0.50, 0.75, 0),
        ("distortion_mid", 196.0, 0.25, 0.65, 0.50, 0.50, 0.75, 0),
        ("distortion_high", 196.0, 0.25, 0.65, 0.95, 0.50, 0.75, 0),
        ("tone_dark", 196.0, 0.12, 0.65, 0.40, 0.00, 0.75, 0),
        ("tone_bright", 196.0, 0.12, 0.65, 0.40, 1.00, 0.75, 0),
        ("bias_alt", 196.0, 0.25, 0.65, 0.50, 0.50, 0.75, 1),
        ("bass_55hz", 55.0, 0.25, 0.65, 0.65, 0.50, 0.75, 0),
        ("bass_82hz", 82.0, 0.25, 0.65, 0.65, 0.50, 0.75, 0),
        ("guitar_196hz", 196.0, 0.25, 0.65, 0.65, 0.50, 0.75, 0),
        ("guitar_440hz", 440.0, 0.25, 0.65, 0.65, 0.50, 0.75, 0),
        ("hard_input", 110.0, 0.75, 0.65, 0.80, 0.50, 0.75, 0),
    )
    for name, frequency, amplitude, boost, distortion, tone, volume, bias in cases:
        csv_path = output_dir / f"case_{name}.csv"
        result = render(
            validator,
            circuit,
            csv_path,
            frequency=frequency,
            amplitude=amplitude,
            boost=boost,
            distortion=distortion,
            tone=tone,
            volume=volume,
            bias=bias,
        )
        if result.returncode != 0:
            safety_pass = False
            report.append(f"| {name} | — | — | — | — | — | — | command failed ({result.returncode}) |")
            continue
        metrics = summarize_output(csv_path, 48000.0, frequency)
        if metrics["convergence_failures"] != 0.0 or not math.isfinite(metrics["peak_fs"]):
            safety_pass = False
        if metrics["peak_fs"] <= 1.0e-10:
            safety_pass = False
        report.append(
            f"| {name} | {metrics['peak_fs']:.6f} | {metrics['rms_fs']:.6f} | "
            f"{metrics['clip_percent']:.3f} | {metrics['thd_percent']:.3f} | "
            f"{metrics['h2_db']:.2f} | {metrics['h3_db']:.2f} | "
            f"{int(metrics['convergence_failures'])} |"
        )

    report.extend([
        "",
        f"**Numerical-safety / non-silence gate:** {'PASS' if safety_pass else 'FAIL'}",
        "",
        "## 3. Sample-rate robustness",
        "",
        "The live application is accepted at 48 kHz / 1x. 44.1 and 96 kHz are",
        "exercised here to catch sample-rate-dependent instability; their exact tone",
        "is reported rather than treated as a golden-response match.",
        "",
        "| Sample rate | Peak FS | RMS FS | Clip % | THD % | Solver failures |",
        "| ---: | ---: | ---: | ---: | ---: | ---: |",
    ])

    sample_rate_pass = True
    for sample_rate in (44100.0, 48000.0, 96000.0):
        csv_path = output_dir / f"sample_rate_{int(sample_rate)}.csv"
        result = render(
            validator,
            circuit,
            csv_path,
            sample_rate=sample_rate,
            frequency=220.0,
            amplitude=0.25,
            boost=0.65,
            distortion=0.65,
            tone=0.50,
            volume=0.75,
        )
        if result.returncode != 0:
            sample_rate_pass = False
            report.append(f"| {sample_rate:.0f} | — | — | — | — | command failed ({result.returncode}) |")
            continue
        metrics = summarize_output(csv_path, sample_rate, 220.0)
        if metrics["convergence_failures"] != 0.0 or metrics["peak_fs"] <= 1.0e-10:
            sample_rate_pass = False
        report.append(
            f"| {sample_rate:.0f} | {metrics['peak_fs']:.6f} | {metrics['rms_fs']:.6f} | "
            f"{metrics['clip_percent']:.3f} | {metrics['thd_percent']:.3f} | "
            f"{int(metrics['convergence_failures'])} |"
        )

    report.extend([
        "",
        f"**44.1/48/96 kHz robustness gate:** {'PASS' if sample_rate_pass else 'FAIL'}",
        "",
        "## 4. Interpretation boundary",
        "",
        "The trace/Aion reference supports the Sziklai Rangemaster front end, dual",
        "100k audio Distortion control, two 1N914 feedback-clipping stages, passive",
        "Big-Muff-style Tone network and linear Volume control. The 10 nF input",
        "coupling capacitor is intentionally retained; this campaign does not require",
        "bass fundamentals to be preserved like a modern bass-specific distortion.",
        "",
        "Original-unit germanium leakage/hFE, 2SC2240 rank, factory BOOST trimmer",
        "setting and production-unit component tolerances remain unverified.",
        "",
    ])

    report_path.write_text("\n".join(report), encoding="utf-8")
    print(report_path.read_text(encoding="utf-8"))
    print(f"Report written to: {report_path}")

    return 0 if boost_pass and safety_pass and sample_rate_pass else 3


if __name__ == "__main__":
    raise SystemExit(main())

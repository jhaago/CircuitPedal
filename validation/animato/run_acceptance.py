#!/usr/bin/env python3
"""Repeatable 1x validation campaign for the Human Gear Animato model.

Hard gates are limited to behaviour supported by the traced circuit or by
numerical-safety requirements. Diagnostic tables quantify the model without
turning subjective preferences into arbitrary DSP targets.
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
AUDIO_VOLTS_PER_FS = 0.20
ANIMATO_OUTPUT_FS_PER_VOLT = 0.50
AUDIO_TAPER_EXPONENT = 3.321928094887362  # conventional 10% value at half rotation


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


def db(value: float, floor: float = -180.0) -> float:
    return 20.0 * math.log10(value) if value > 0.0 else floor


def harmonic_amplitudes(
    values: list[float], sample_rate: float, fundamental: float, count: int = 6
) -> list[float]:
    """Return targeted sinusoidal amplitudes after removing DC."""
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


def basic_metrics(path: Path) -> dict[str, float]:
    values, failures = read_column(path, "output_fs")
    peak = max(abs(value) for value in values)
    positive_peak = max(values)
    negative_peak = abs(min(values))
    clipped = sum(1 for value in values if abs(value) >= 0.999999)
    asymmetry_db = (
        db(positive_peak / negative_peak)
        if positive_peak > 1.0e-15 and negative_peak > 1.0e-15
        else 0.0
    )
    return {
        "peak_fs": peak,
        "rms_fs": rms(values),
        "clip_percent": 100.0 * clipped / len(values),
        "convergence_failures": float(failures),
        "positive_peak_fs": positive_peak,
        "negative_peak_fs": negative_peak,
        "asymmetry_db": asymmetry_db,
    }


def summarize_output(path: Path, sample_rate: float, fundamental: float) -> dict[str, float]:
    metrics = basic_metrics(path)
    values, _ = read_column(path, "output_fs")
    harmonics = harmonic_amplitudes(values, sample_rate, fundamental)
    fundamental_amp = harmonics[0]
    harmonic_power = sum(amplitude * amplitude for amplitude in harmonics[1:])
    thd = math.sqrt(harmonic_power) / fundamental_amp if fundamental_amp > 1.0e-15 else 0.0
    metrics.update({
        "fundamental": fundamental_amp,
        "thd_percent": 100.0 * thd,
        "h2_db": db(harmonics[1] / fundamental_amp) if fundamental_amp > 1.0e-15 else -180.0,
        "h3_db": db(harmonics[2] / fundamental_amp) if fundamental_amp > 1.0e-15 else -180.0,
    })
    return metrics


def render(
    validator: Path,
    circuit: Path,
    output: Path,
    *,
    sample_rate: float = 48000.0,
    signal: str = "sine",
    frequency: float = 196.0,
    frequency2: float | None = None,
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
        "--signal", signal,
        "--frequency", str(frequency),
        "--amplitude", str(amplitude),
        "--control", f"BOOST={boost}",
        "--control", f"DISTORTION={distortion}",
        "--control", f"TONE={tone}",
        "--control", f"VOLUME={volume}",
        "--switch", f"BIAS={bias}",
    ]
    if frequency2 is not None:
        command.extend(["--frequency2", str(frequency2)])
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
        "This campaign exercises the component-level GenericCircuit solver used by",
        "the accepted live 1x path. It does not use the experimental 4x wrapper and",
        "does not add a clean blend or artificial bass preservation.",
        "",
        "## 1. Confirmed control-orientation gate",
        "",
    ]

    boost_rows: list[tuple[str, float, float, int]] = []
    boost_pass = True
    for name, position in (("minimum", 0.0), ("maximum", 1.0)):
        csv_path = output_dir / f"boost_{name}.csv"
        result = render(
            validator, circuit, csv_path,
            frequency=440.0, amplitude=0.05, boost=position,
            distortion=0.65, tone=0.50, volume=0.55,
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
    report.extend(["", f"**BOOST direction gate:** {'PASS' if boost_pass else 'FAIL'}", ""])

    report.extend([
        "## 2. Gain / clipping baseline",
        "",
        "Input amplitude 0.25 FS corresponds to 50 mV peak at the model's 0.20 V/FS",
        "input calibration. Full-scale digital clipping is a hard failure for these",
        "normal/stress cases; analogue transistor/diode clipping is expected.",
        "",
        "| State | Peak FS | RMS FS | Clip % | THD % | H2/fund dB | H3/fund dB | Peak asym dB | Solver failures |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ])

    safety_pass = True
    baseline_metrics: dict[str, dict[str, float]] = {}
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
            validator, circuit, csv_path,
            frequency=frequency, amplitude=amplitude, boost=boost,
            distortion=distortion, tone=tone, volume=volume, bias=bias,
        )
        if result.returncode != 0:
            safety_pass = False
            report.append(f"| {name} | — | — | — | — | — | — | — | command failed ({result.returncode}) |")
            continue
        metrics = summarize_output(csv_path, 48000.0, frequency)
        baseline_metrics[name] = metrics
        if (
            metrics["convergence_failures"] != 0.0
            or not math.isfinite(metrics["peak_fs"])
            or metrics["peak_fs"] <= 1.0e-10
            or metrics["clip_percent"] > 0.0
        ):
            safety_pass = False
        report.append(
            f"| {name} | {metrics['peak_fs']:.6f} | {metrics['rms_fs']:.6f} | "
            f"{metrics['clip_percent']:.3f} | {metrics['thd_percent']:.3f} | "
            f"{metrics['h2_db']:.2f} | {metrics['h3_db']:.2f} | "
            f"{metrics['asymmetry_db']:+.2f} | {int(metrics['convergence_failures'])} |"
        )
    report.extend(["", f"**Numerical-safety / digital-headroom gate:** {'PASS' if safety_pass else 'FAIL'}", ""])

    report.extend([
        "## 3. Output-domain calibration check",
        "",
        "This separates analogue circuit level from the final host-domain conversion",
        "instead of assuming another pedal's OUTPUT multiplier should be copied.",
        "",
        "| Input peak | Input RMS (analogue) | OUT RMS (analogue) | Output RMS (FS) | Measured FS/V | Analogue RMS gain |",
        "| ---: | ---: | ---: | ---: | ---: | ---: |",
    ])
    calibration_pass = True
    calibration_csv = output_dir / "output_calibration.csv"
    calibration_result = render(
        validator, circuit, calibration_csv,
        frequency=196.0, amplitude=0.25, boost=0.65,
        distortion=0.50, tone=0.50, volume=0.75, nodes=("OUT",),
    )
    if calibration_result.returncode != 0:
        calibration_pass = False
        report.append("| 50 mV | — | — | — | — | command failed |")
    else:
        out_node, failures = read_column(calibration_csv, "node_OUT_v")
        output_values, output_failures = read_column(calibration_csv, "output_fs")
        out_rms_v = ac_rms(out_node)
        output_rms_fs = ac_rms(output_values)
        input_rms_v = 0.25 * AUDIO_VOLTS_PER_FS / math.sqrt(2.0)
        measured_scale = output_rms_fs / out_rms_v if out_rms_v > 1.0e-15 else 0.0
        analogue_gain = out_rms_v / input_rms_v if input_rms_v > 0.0 else 0.0
        if failures or output_failures or abs(measured_scale - ANIMATO_OUTPUT_FS_PER_VOLT) > 0.002:
            calibration_pass = False
        report.append(
            f"| 50 mV | {input_rms_v:.6f} V | {out_rms_v:.6f} V | {output_rms_fs:.6f} | "
            f"{measured_scale:.6f} | {analogue_gain:.2f}x ({db(analogue_gain):+.2f} dB) |"
        )
    report.extend([
        "",
        f"**Output conversion consistency gate:** {'PASS' if calibration_pass else 'FAIL'}",
        "",
        "The conversion gate checks implementation consistency only. Whether 0.5 FS/V is",
        "appropriate is judged alongside the headroom/level table above; it is not changed",
        "merely to match a different pedal model.",
        "",
    ])

    report.extend([
        "## 4. Dual-gang DISTORTION law",
        "",
        "The traced pedal uses a dual 100kA control. CircuitPedal's LOG law maps physical",
        "rotation to the conventional ~10% electrical fraction at half rotation. Because",
        "both attenuating gangs move together, the low end of the control can be extremely",
        "quiet without implying a routing or output-calibration fault.",
        "",
        "| Rotation | Per-gang electrical fraction | Output RMS FS | Relative to max | Solver failures |",
        "| ---: | ---: | ---: | ---: | ---: |",
    ])
    drive_rows: list[tuple[float, float, float, int]] = []
    for position in (0.0, 0.10, 0.25, 0.50, 0.75, 1.00):
        csv_path = output_dir / f"drive_{int(position * 100):03d}.csv"
        result = render(
            validator, circuit, csv_path,
            frequency=196.0, amplitude=0.25, boost=0.65,
            distortion=position, tone=0.50, volume=0.75,
        )
        if result.returncode != 0:
            drive_rows.append((position, 0.0, 0.0, -1))
            safety_pass = False
            continue
        values, failures = read_column(csv_path, "output_fs")
        drive_rows.append((position, position ** AUDIO_TAPER_EXPONENT, ac_rms(values), failures))
        if failures:
            safety_pass = False
    max_drive_rms = max((row[2] for row in drive_rows), default=0.0)
    for position, fraction, level, failures in drive_rows:
        relative = db(level / max_drive_rms) if max_drive_rms > 0.0 and level > 0.0 else -180.0
        report.append(f"| {position:.2f} | {fraction:.6f} | {level:.9f} | {relative:.2f} dB | {failures} |")
    report.append("")

    report.extend([
        "## 5. Low-level frequency response",
        "",
        "A 0.5 mV-peak analogue sine is used to reduce nonlinear level dependence. The",
        "BOOST_W column isolates the Rangemaster/front-end trend; output fundamental gain",
        "includes the rest of the pedal at center Tone. These are model measurements, not",
        "claimed original-unit lab data.",
        "",
        "| Frequency | BOOST_W gain | Output fundamental gain | Output THD | Solver failures |",
        "| ---: | ---: | ---: | ---: | ---: |",
    ])
    response_pass = True
    low_amplitude_fs = 0.0025
    input_rms_v = low_amplitude_fs * AUDIO_VOLTS_PER_FS / math.sqrt(2.0)
    for frequency in (40.0, 55.0, 82.0, 110.0, 196.0, 440.0, 1000.0, 2000.0, 5000.0, 10000.0):
        csv_path = output_dir / f"response_{int(frequency):05d}.csv"
        result = render(
            validator, circuit, csv_path,
            frequency=frequency, amplitude=low_amplitude_fs,
            boost=1.0, distortion=0.50, tone=0.50, volume=0.75,
            nodes=("BOOST_W",),
        )
        if result.returncode != 0:
            response_pass = False
            report.append(f"| {frequency:.0f} Hz | — | — | — | command failed |")
            continue
        boost_node, failures = read_column(csv_path, "node_BOOST_W_v")
        metrics = summarize_output(csv_path, 48000.0, frequency)
        boost_gain = ac_rms(boost_node) / input_rms_v if input_rms_v > 0.0 else 0.0
        output_gain = metrics["fundamental"] / low_amplitude_fs if low_amplitude_fs > 0.0 else 0.0
        if failures or metrics["convergence_failures"] != 0.0:
            response_pass = False
        report.append(
            f"| {frequency:.0f} Hz | {db(boost_gain):+.2f} dB | {db(output_gain):+.2f} dB | "
            f"{metrics['thd_percent']:.3f}% | {failures + int(metrics['convergence_failures'])} |"
        )
    report.extend(["", f"**Frequency-sweep numerical gate:** {'PASS' if response_pass else 'FAIL'}", ""])

    report.extend([
        "## 6. Tone-network direction",
        "",
        "The passive Tone control is expected to pan between low-pass and high-pass",
        "branches, not act as a generic treble shelf.",
        "",
        "| Tone | 110 Hz gain | 440 Hz gain | 2 kHz gain | 5 kHz gain |",
        "| ---: | ---: | ---: | ---: | ---: |",
    ])
    tone_rows: dict[float, list[float]] = {}
    for tone in (0.0, 0.5, 1.0):
        gains: list[float] = []
        for frequency in (110.0, 440.0, 2000.0, 5000.0):
            csv_path = output_dir / f"tone_{int(tone * 100):03d}_{int(frequency):05d}.csv"
            result = render(
                validator, circuit, csv_path,
                frequency=frequency, amplitude=low_amplitude_fs,
                boost=1.0, distortion=0.50, tone=tone, volume=0.75,
            )
            if result.returncode != 0:
                response_pass = False
                gains.append(float("nan"))
                continue
            metrics = summarize_output(csv_path, 48000.0, frequency)
            if metrics["convergence_failures"] != 0.0:
                response_pass = False
            gains.append(db(metrics["fundamental"] / low_amplitude_fs))
        tone_rows[tone] = gains
    for tone, gains in tone_rows.items():
        formatted = [f"{value:+.2f} dB" if math.isfinite(value) else "—" for value in gains]
        report.append(f"| {tone:.1f} | {formatted[0]} | {formatted[1]} | {formatted[2]} | {formatted[3]} |")
    report.append("")

    report.extend([
        "## 7. Transient / multi-frequency stress",
        "",
        "These cases are not substitutes for recorded guitar/bass A/B tests, but they",
        "exercise abrupt and multi-frequency inputs to catch solver instability or host",
        "hard clipping that a steady sine could miss.",
        "",
        "| Signal | Peak FS | RMS FS | Clip % | Peak asym dB | Solver failures |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ])
    transient_pass = True
    stress_cases = (
        ("step", "step", 110.0, None, 0.25),
        ("impulse", "impulse", 110.0, None, 0.75),
        ("dual_82_659", "dualtone", 82.0, 659.0, 0.20),
        ("dual_196_1760", "dualtone", 196.0, 1760.0, 0.20),
    )
    for name, signal, frequency, frequency2, amplitude in stress_cases:
        csv_path = output_dir / f"stress_{name}.csv"
        result = render(
            validator, circuit, csv_path,
            signal=signal, frequency=frequency, frequency2=frequency2,
            amplitude=amplitude, boost=0.65, distortion=0.80,
            tone=0.50, volume=0.75,
        )
        if result.returncode != 0:
            transient_pass = False
            report.append(f"| {name} | — | — | — | — | command failed |")
            continue
        metrics = basic_metrics(csv_path)
        if metrics["convergence_failures"] != 0.0 or metrics["clip_percent"] > 0.0:
            transient_pass = False
        report.append(
            f"| {name} | {metrics['peak_fs']:.6f} | {metrics['rms_fs']:.6f} | "
            f"{metrics['clip_percent']:.3f} | {metrics['asymmetry_db']:+.2f} | "
            f"{int(metrics['convergence_failures'])} |"
        )
    report.extend(["", f"**Transient / multi-frequency safety gate:** {'PASS' if transient_pass else 'FAIL'}", ""])

    report.extend([
        "## 8. Sample-rate robustness",
        "",
        "The live application is accepted at 48 kHz / 1x. 44.1 and 96 kHz are",
        "exercised to catch sample-rate-dependent instability; exact tone is reported",
        "rather than treated as a golden-response match.",
        "",
        "| Sample rate | Peak FS | RMS FS | Clip % | THD % | Solver failures |",
        "| ---: | ---: | ---: | ---: | ---: | ---: |",
    ])
    sample_rate_pass = True
    for sample_rate in (44100.0, 48000.0, 96000.0):
        csv_path = output_dir / f"sample_rate_{int(sample_rate)}.csv"
        result = render(
            validator, circuit, csv_path,
            sample_rate=sample_rate, frequency=220.0, amplitude=0.25,
            boost=0.65, distortion=0.65, tone=0.50, volume=0.75,
        )
        if result.returncode != 0:
            sample_rate_pass = False
            report.append(f"| {sample_rate:.0f} | — | — | — | — | command failed ({result.returncode}) |")
            continue
        metrics = summarize_output(csv_path, sample_rate, 220.0)
        if (
            metrics["convergence_failures"] != 0.0
            or metrics["peak_fs"] <= 1.0e-10
            or metrics["clip_percent"] > 0.0
        ):
            sample_rate_pass = False
        report.append(
            f"| {sample_rate:.0f} | {metrics['peak_fs']:.6f} | {metrics['rms_fs']:.6f} | "
            f"{metrics['clip_percent']:.3f} | {metrics['thd_percent']:.3f} | "
            f"{int(metrics['convergence_failures'])} |"
        )
    report.extend(["", f"**44.1/48/96 kHz robustness gate:** {'PASS' if sample_rate_pass else 'FAIL'}", ""])

    report.extend([
        "## 9. Interpretation boundary",
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

    all_pass = (
        boost_pass
        and safety_pass
        and calibration_pass
        and response_pass
        and transient_pass
        and sample_rate_pass
    )
    return 0 if all_pass else 3


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""48 kHz / 1x sanity checks for every exposed Animato control."""

from __future__ import annotations

import csv
import math
import subprocess
import sys
import tempfile
from pathlib import Path


def rms(path: Path) -> float:
    values: list[float] = []
    failures = 0
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            value = float(row["output_fs"])
            if not math.isfinite(value):
                raise RuntimeError(f"non-finite output in {path}")
            values.append(value)
            if row.get("converged") == "0":
                failures += 1
    if failures:
        raise RuntimeError(f"{failures} nonlinear solve failures in {path}")
    if not values:
        raise RuntimeError(f"no output samples in {path}")
    analysis_samples = 48000
    if len(values) < analysis_samples:
        raise RuntimeError(f"capture is too short for steady-state analysis: {path}")
    values = values[-analysis_samples:]
    mean = sum(values) / len(values)
    return math.sqrt(
        sum((value - mean) ** 2 for value in values) / len(values)
    )


def render(
    validator: Path,
    circuit: Path,
    output: Path,
    *,
    frequency: float,
    amplitude: float = 0.10,
    boost: float = 0.65,
    distortion: float = 0.65,
    tone: float = 0.50,
    volume: float = 0.75,
    bias: int = 0,
) -> float:
    command = [
        str(validator), "render", str(circuit), str(output),
        "--sample-rate", "48000",
        "--seconds", "4.0",
        "--warmup", "0.06",
        "--signal", "sine",
        "--frequency", str(frequency),
        "--amplitude", str(amplitude),
        "--control", f"BOOST={boost}",
        "--control", f"DISTORTION={distortion}",
        "--control", f"TONE={tone}",
        "--control", f"VOLUME={volume}",
        "--switch", f"BIAS={bias}",
    ]
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise RuntimeError(f"render failed: {' '.join(command)}")
    return rms(output)


def db_ratio(high: float, low: float) -> float:
    if high <= 0.0 or low <= 0.0:
        return float("-inf")
    return 20.0 * math.log10(high / low)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_controls.py <circuitpedal_validate>", file=sys.stderr)
        return 2

    validator = Path(sys.argv[1]).resolve()
    repo = Path(__file__).resolve().parents[2]
    circuit = repo / "circuits" / "animato_reference_draft.cpedal"
    failures: list[str] = []

    with tempfile.TemporaryDirectory(prefix="animato-controls-") as temp:
        directory = Path(temp)

        boost_low = render(validator, circuit, directory / "boost_low.csv",
                           frequency=440.0, boost=0.15)
        boost_high = render(validator, circuit, directory / "boost_high.csv",
                            frequency=440.0, boost=0.85)
        # BOOST acts before multiple clipping/compression stages, so a large
        # internal level change does not need to survive linearly to the final
        # output. The main acceptance campaign separately checks BOOST_W itself;
        # this test only requires a clear (>~0.8 dB) audible-output consequence.
        if not boost_high > boost_low * 1.10:
            failures.append("BOOST did not measurably increase final signal level")

        distortion_low = render(validator, circuit, directory / "dist_low.csv",
                                frequency=196.0, distortion=0.25)
        distortion_high = render(validator, circuit, directory / "dist_high.csv",
                                 frequency=196.0, distortion=0.75)
        if not distortion_high > distortion_low * 2.0:
            failures.append("DISTORTION did not increase drive/output as expected")

        # Tone is a Muff-style low-pass/high-pass pan. Check both sides of its
        # crossover rather than reducing it to a single broadband level test.
        tone_dark_low = render(validator, circuit, directory / "tone_dark_110.csv",
                               frequency=110.0, amplitude=0.01, tone=0.0)
        tone_bright_low = render(validator, circuit, directory / "tone_bright_110.csv",
                                 frequency=110.0, amplitude=0.01, tone=1.0)
        tone_dark_high = render(validator, circuit, directory / "tone_dark_5k.csv",
                                frequency=5000.0, amplitude=0.01, tone=0.0)
        tone_bright_high = render(validator, circuit, directory / "tone_bright_5k.csv",
                                  frequency=5000.0, amplitude=0.01, tone=1.0)
        if not tone_dark_low > tone_bright_low * 1.15:
            failures.append("TONE did not favor low frequencies at the dark end")
        if not tone_bright_high > tone_dark_high * 2.0:
            failures.append("TONE did not favor high frequencies at the bright end")

        volume_low = render(validator, circuit, directory / "volume_low.csv",
                            frequency=196.0, volume=0.25)
        volume_high = render(validator, circuit, directory / "volume_high.csv",
                             frequency=196.0, volume=0.75)
        if not volume_high > volume_low * 2.0:
            failures.append("VOLUME did not increase output level")

        bias_normal = render(validator, circuit, directory / "bias_normal.csv",
                             frequency=196.0, distortion=0.50, bias=0)
        bias_alt = render(validator, circuit, directory / "bias_alt.csv",
                          frequency=196.0, distortion=0.50, bias=1)
        bias_change_db = abs(db_ratio(bias_alt, bias_normal))
        if not (0.10 < bias_change_db < 6.0):
            failures.append(
                f"BIAS level shift was not modest-but-real ({bias_change_db:.3f} dB)"
            )

        print("Animato control-effect sanity (48 kHz / 1x)")
        print(f"  BOOST 0.15 -> 0.85: {boost_low:.9f} -> {boost_high:.9f} FS RMS")
        print(f"  DISTORTION 0.25 -> 0.75: {distortion_low:.9f} -> {distortion_high:.9f} FS RMS")
        print(
            "  TONE 0 -> 1: "
            f"110 Hz {tone_dark_low:.9f} -> {tone_bright_low:.9f}, "
            f"5 kHz {tone_dark_high:.9f} -> {tone_bright_high:.9f} FS RMS"
        )
        print(f"  VOLUME 0.25 -> 0.75: {volume_low:.9f} -> {volume_high:.9f} FS RMS")
        print(
            f"  BIAS normal -> alt: {bias_normal:.9f} -> {bias_alt:.9f} FS RMS "
            f"({db_ratio(bias_alt, bias_normal):+.3f} dB)"
        )

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 3

    print("PASS: every exposed Animato control has a measurable, circuit-sensible effect")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

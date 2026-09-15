#!/usr/bin/env python3
"""Unit checks for Animato steady-state measurement helpers."""

from __future__ import annotations

import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from run_acceptance import (  # noqa: E402
    harmonic_count_below_nyquist,
    steady_state_samples,
    waveform_metrics,
)


class AnalysisHelpersTest(unittest.TestCase):
    def test_steady_state_window_excludes_startup(self) -> None:
        sample_rate = 100
        startup = [-0.75] * 200
        settled = [0.25 * math.sin(2.0 * math.pi * 5.0 * n / sample_rate)
                   for n in range(sample_rate)]
        self.assertEqual(
            steady_state_samples(startup + settled, sample_rate, 1.0),
            settled,
        )

    def test_waveform_metrics_separate_dc_from_ac_shape(self) -> None:
        sample_rate = 1000
        values = [
            0.20 + 0.50 * math.sin(2.0 * math.pi * 10.0 * n / sample_rate)
            for n in range(sample_rate)
        ]
        metrics = waveform_metrics(values)
        self.assertAlmostEqual(metrics["dc_offset_fs"], 0.20, places=12)
        self.assertAlmostEqual(metrics["ac_rms_fs"], 0.50 / math.sqrt(2.0), places=12)
        self.assertAlmostEqual(metrics["positive_peak_fs"], 0.50, places=12)
        self.assertAlmostEqual(metrics["negative_peak_fs"], 0.50, places=12)
        self.assertAlmostEqual(metrics["asymmetry_db"], 0.0, places=12)

    def test_harmonics_stop_below_nyquist(self) -> None:
        self.assertEqual(harmonic_count_below_nyquist(48000.0, 1000.0, 6), 6)
        self.assertEqual(harmonic_count_below_nyquist(48000.0, 10000.0, 6), 2)
        with self.assertRaises(ValueError):
            harmonic_count_below_nyquist(44100.0, 22050.0, 6)


if __name__ == "__main__":
    unittest.main()

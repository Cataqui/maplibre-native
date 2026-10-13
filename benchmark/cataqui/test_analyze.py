import unittest
from analyze import _compare, _resolved

class PairedMeasurements(unittest.TestCase):
    def _samples(self, factor):
        return [{"case": "fixture", "phases": {"cold": value * factor, "warm": value * factor}} for value in [9.9, 10.0, 10.1, 10.0] * 3]
    def test_identical_measurements_do_not_claim_an_improvement(self):
        result = _compare(self._samples(1), self._samples(1))
        self.assertEqual(result["scoreRatio"], 1)
        self.assertLessEqual(result["confidence95"][0], 1)
        self.assertGreaterEqual(result["confidence95"][1], 1)
    def test_a_large_regression_has_a_confidence_interval_above_one(self):
        result = _compare(self._samples(1), self._samples(1.2))
        self.assertGreater(result["confidence95"][0], 1)
    def test_a_clear_speedup_has_a_confidence_interval_below_one(self):
        result = _compare(self._samples(1), self._samples(0.8))
        self.assertLess(result["confidence95"][1], 1)
    def test_missing_workloads_are_rejected_instead_of_looking_faster(self):
        with self.assertRaises(ValueError):
            _compare(self._samples(1), [{"case": "fixture", "phases": {"cold": 1}}])
    def test_successful_retry_resolves_failure_regardless_of_file_order(self):
        for reverse in (False, True):
            failed = {"sha": "version", "status": "failed", "error": "build"}
            measured = {"sha": "version", "status": "measured", "scoreRatio": 1}
            result = _resolved([failed, measured] if not reverse else [measured, failed])
            self.assertEqual(len(result), 1)
            self.assertEqual(result[0]["status"], "measured")
            self.assertEqual(result[0]["previousFailures"], [failed])
    def test_independent_repeats_cannot_silently_overwrite_measurements(self):
        with self.assertRaisesRegex(ValueError, "independent measured"):
            _resolved([{"sha": "version", "status": "measured"}, {"sha": "version", "status": "measured"}])

if __name__ == "__main__":
    unittest.main()

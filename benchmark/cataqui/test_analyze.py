import unittest
from analyze import _compare

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

if __name__ == "__main__":
    unittest.main()

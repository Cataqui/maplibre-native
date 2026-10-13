import unittest

import numpy as np

from visual_compare import _difference


class VisualDifferenceTest(unittest.TestCase):
    def test_single_channel_change_counts_one_pixel(self):
        reference = np.zeros((2, 2, 4), dtype=np.uint8)
        candidate = reference.copy()
        candidate[0, 0, 0] = 255
        result = _difference(reference, candidate)
        self.assertEqual(result["changedPixelFraction"], 0.25)
        self.assertEqual(result["maximumChannelDifference"], 255)
        self.assertEqual(result["meanAbsoluteChannelDifference"], 255 / 16)

    def test_uint8_subtraction_does_not_wrap(self):
        reference = np.full((1, 1, 4), 255, dtype=np.uint8)
        candidate = np.zeros((1, 1, 4), dtype=np.uint8)
        self.assertEqual(_difference(reference, candidate)["meanAbsoluteChannelDifference"], 255)

    def test_resized_output_requires_review(self):
        with self.assertRaisesRegex(ValueError, "dimensions differ"):
            _difference(np.zeros((2, 2, 4)), np.zeros((3, 2, 4)))


if __name__ == "__main__":
    unittest.main()

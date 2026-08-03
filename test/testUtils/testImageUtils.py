#
# Copyright 2026 Autodesk
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Standalone unit tests for imageUtils helpers that do not need Maya."""

import os
import tempfile
import unittest

from PySide6.QtGui import QImage, QColor

import imageUtils


def _writeTestImage(path, fill_color):
    image = QImage(8, 8, QImage.Format.Format_RGB32)
    image.fill(fill_color)
    if not image.save(path):
        raise RuntimeError("Failed to write test image: {}".format(path))


def _writeHalfTransparentTestImage(path, opaque_color, transparent_color):
    """Write an image whose top half is opaque_color (alpha=255) and whose
    bottom half is transparent_color with alpha=0."""
    image = QImage(8, 8, QImage.Format.Format_ARGB32)
    for y in range(8):
        for x in range(8):
            color = opaque_color if y < 4 else transparent_color
            image.setPixelColor(x, y, color)
    if not image.save(path):
        raise RuntimeError("Failed to write test image: {}".format(path))


class ImageUtilsTestCase(unittest.TestCase):
    def test_imageMeanLuminance_black_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "black.png")
            _writeTestImage(path, QColor(0, 0, 0))
            self.assertEqual(imageUtils.imageMeanLuminance(path), 0.0)

    def test_imageMeanLuminance_non_black_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "gray.png")
            _writeTestImage(path, QColor(128, 128, 128))
            mean = imageUtils.imageMeanLuminance(path)
            self.assertGreater(mean, imageUtils.SNAPSHOT_MIN_MEAN_LUMINANCE)

    def test_imageMeanLuminance_excludes_fully_transparent_pixels(self):
        # Regression: the mean must be computed over opaque pixels only, not
        # diluted by (or divided by the count of) fully-transparent ones --
        # this is what lets assertImageNonBlank tell an empty AOV/playblast
        # apart from a normally-composited image with a transparent border.
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "half_transparent.png")
            opaque = QColor(200, 200, 200, 255)
            transparent = QColor(0, 0, 0, 0)
            _writeHalfTransparentTestImage(path, opaque, transparent)
            mean = imageUtils.imageMeanLuminance(path)
            expected = (
                0.2126 * opaque.redF() + 0.7152 * opaque.greenF() + 0.0722 * opaque.blueF()
            )
            self.assertAlmostEqual(mean, expected, places=3)

    def test_imageMeanLuminance_fully_transparent_image_returns_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "transparent.png")
            image = QImage(8, 8, QImage.Format.Format_ARGB32)
            image.fill(QColor(255, 255, 255, 0))
            self.assertTrue(image.save(path))
            self.assertEqual(imageUtils.imageMeanLuminance(path), 0.0)

    def test_assertImageNonBlank_rejects_black_snapshot(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "black.png")
            _writeTestImage(path, QColor(0, 0, 0))
            with self.assertRaises(AssertionError):
                imageUtils.assertImageNonBlank(path)

    def test_assertImageNonBlank_accepts_non_black_snapshot(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "gray.png")
            _writeTestImage(path, QColor(64, 64, 64))
            imageUtils.assertImageNonBlank(path)


if __name__ == "__main__":
    unittest.main(verbosity=2)

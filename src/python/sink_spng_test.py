#!/usr/bin/env python3
# Copyright 2025 Jonas Teuwen. All Rights Reserved.
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
"""
Tests for SpngSink streaming PNG encoder.

This module tests the Python bindings for SpngSink, which provides
streaming PNG encoding with minimal memory usage.
"""

import os
import tempfile

import numpy as np
import pytest

import fim


@pytest.fixture
def temp_dir():
    """Create a temporary directory for test outputs."""
    tmp = tempfile.mkdtemp()
    yield tmp
    import shutil

    if os.path.exists(tmp):
        shutil.rmtree(tmp)


class TestSpngSink:
    """Tests for SpngSink streaming PNG encoder."""

    def test_write_spng_rgb(self, temp_dir):
        """Test writing RGB image with SpngSink."""
        # Create a simple RGB image
        data = np.zeros((50, 40, 3), dtype=np.uint8)
        # Create a gradient pattern
        for y in range(50):
            for x in range(40):
                data[y, x, 0] = (x * 255) // 40  # Red gradient
                data[y, x, 1] = (y * 255) // 50  # Green gradient
                data[y, x, 2] = 128  # Constant blue

        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 40, 50, 3)
        output_path = os.path.join(temp_dir, "test_rgb.png")
        img.write_spng(output_path)

        # Verify file was created and has reasonable size
        assert os.path.exists(output_path)
        assert os.path.getsize(output_path) > 100

    def test_write_spng_rgba(self, temp_dir):
        """Test writing RGBA image with SpngSink."""
        # Create an RGBA image with alpha channel
        data = np.ones((60, 50, 4), dtype=np.uint8) * 200
        # Set varying alpha values
        for y in range(60):
            data[y, :, 3] = (y * 255) // 60

        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 50, 60, 4)
        output_path = os.path.join(temp_dir, "test_rgba.png")
        img.write_spng(output_path)

        assert os.path.exists(output_path)
        assert os.path.getsize(output_path) > 100

    def test_write_spng_grayscale(self, temp_dir):
        """Test writing grayscale image with SpngSink."""
        # Create a grayscale gradient
        data = np.zeros((30, 25, 1), dtype=np.uint8)
        for y in range(30):
            for x in range(25):
                data[y, x, 0] = ((x + y) * 255) // 55

        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 25, 30, 1)
        output_path = os.path.join(temp_dir, "test_gray.png")
        img.write_spng(output_path)

        assert os.path.exists(output_path)
        assert os.path.getsize(output_path) > 50

    def test_write_spng_large_image(self, temp_dir):
        """Test SpngSink with large image to verify streaming works."""
        # Create a large image (1000x800 RGB)
        # This tests that streaming encoding doesn't use excessive memory
        data = np.random.randint(0, 256, (800, 1000, 3), dtype=np.uint8)

        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 1000, 800, 3)
        output_path = os.path.join(temp_dir, "test_large.png")
        img.write_spng(output_path)

        assert os.path.exists(output_path)
        # Compressed PNG should be significantly smaller than raw data
        assert os.path.getsize(output_path) > 1000

    def test_write_spng_small_image(self, temp_dir):
        """Test SpngSink with minimal 1x1 image."""
        data = np.array([[[255, 0, 128]]], dtype=np.uint8)

        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 1, 1, 3)
        output_path = os.path.join(temp_dir, "test_small.png")
        img.write_spng(output_path)

        assert os.path.exists(output_path)
        assert os.path.getsize(output_path) > 0

    def test_write_spng_non_power_of_two(self, temp_dir):
        """Test SpngSink with non-power-of-2 dimensions."""
        # Use odd dimensions that aren't powers of 2
        data = np.ones((123, 87, 3), dtype=np.uint8) * 150

        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 87, 123, 3)
        output_path = os.path.join(temp_dir, "test_odd_dims.png")
        img.write_spng(output_path)

        assert os.path.exists(output_path)
        assert os.path.getsize(output_path) > 100

    def test_write_spng_with_crop(self, temp_dir):
        """Test SpngSink with cropped image (lazy evaluation)."""
        # Create image and crop it
        data = np.random.randint(0, 256, (100, 100, 3), dtype=np.uint8)
        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 100, 100, 3)
        cropped = img.crop((20, 20), (60, 60))

        output_path = os.path.join(temp_dir, "test_cropped.png")
        cropped.write_spng(output_path)

        assert os.path.exists(output_path)
        assert os.path.getsize(output_path) > 100

    def test_write_spng_with_chained_operations(self, temp_dir):
        """Test SpngSink with chained operations."""
        # Test that SpngSink works correctly with lazy pipeline
        data = np.random.randint(0, 256, (200, 200, 3), dtype=np.uint8)
        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 200, 200, 3)

        # Chain multiple operations
        result = img.crop((10, 10), (180, 180)).downsample(2)

        output_path = os.path.join(temp_dir, "test_chained.png")
        result.write_spng(output_path)

        assert os.path.exists(output_path)
        assert os.path.getsize(output_path) > 100

    def test_write_spng_vs_write_png(self, temp_dir):
        """Test that SpngSink and LodePngSink produce valid outputs."""
        # Create same image and write with both sinks
        data = np.random.randint(0, 256, (100, 100, 3), dtype=np.uint8)

        flat_data = data.flatten().tolist()
        img1 = fim.Image.from_memory(flat_data, 100, 100, 3)
        img2 = fim.Image.from_memory(flat_data, 100, 100, 3)

        spng_path = os.path.join(temp_dir, "output_spng.png")
        lodepng_path = os.path.join(temp_dir, "output_lodepng.png")

        img1.write_spng(spng_path)
        img2.write_png(lodepng_path)

        # Both files should exist and be valid PNGs
        assert os.path.exists(spng_path)
        assert os.path.exists(lodepng_path)
        assert os.path.getsize(spng_path) > 1000
        assert os.path.getsize(lodepng_path) > 1000

        # Read PNG headers to verify they're valid
        with open(spng_path, "rb") as f:
            png_sig = f.read(8)
            assert png_sig == b"\x89PNG\r\n\x1a\n"

        with open(lodepng_path, "rb") as f:
            png_sig = f.read(8)
            assert png_sig == b"\x89PNG\r\n\x1a\n"

    def test_roundtrip_rgb(self, temp_dir):
        """Test writing and reading back RGB image to verify correctness."""
        # Create a known pattern
        data = np.zeros((50, 40, 3), dtype=np.uint8)
        for y in range(50):
            for x in range(40):
                data[y, x, 0] = (x * 255) // 40  # Red gradient
                data[y, x, 1] = (y * 255) // 50  # Green gradient
                data[y, x, 2] = 128  # Constant blue

        # Write with SpngSink
        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 40, 50, 3)
        output_path = os.path.join(temp_dir, "roundtrip_rgb.png")
        img.write_spng(output_path)

        # Read back
        img_read = fim.Image.from_png(output_path)
        data_read = img_read.to_numpy()

        # Verify dimensions
        assert data_read.shape == data.shape

        # Verify data matches (should be exact for lossless PNG)
        np.testing.assert_array_equal(data_read, data)

    def test_roundtrip_rgba(self, temp_dir):
        """Test writing and reading back RGBA image to verify alpha channel."""
        # Create RGBA image with varying alpha
        data = np.ones((30, 25, 4), dtype=np.uint8) * 200
        for y in range(30):
            data[y, :, 3] = (y * 255) // 30  # Alpha gradient

        # Write and read back
        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 25, 30, 4)
        output_path = os.path.join(temp_dir, "roundtrip_rgba.png")
        img.write_spng(output_path)

        img_read = fim.Image.from_png(output_path)
        data_read = img_read.to_numpy()

        # Verify exact match
        assert data_read.shape == (30, 25, 4)
        np.testing.assert_array_equal(data_read, data)

    def test_roundtrip_grayscale(self, temp_dir):
        """Test writing and reading back grayscale image."""
        # Create grayscale gradient
        data = np.zeros((40, 35, 1), dtype=np.uint8)
        for y in range(40):
            for x in range(35):
                data[y, x, 0] = ((x + y) * 255) // 75

        # Write and read back
        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 35, 40, 1)
        output_path = os.path.join(temp_dir, "roundtrip_gray.png")
        img.write_spng(output_path)

        img_read = fim.Image.from_png(output_path)
        data_read = img_read.to_numpy()

        # Verify exact match
        assert data_read.shape == (40, 35, 1)
        np.testing.assert_array_equal(data_read, data)

    def test_roundtrip_with_operations(self, temp_dir):
        """Test roundtrip with lazy operations applied before writing."""
        # Create image, apply operations, write, read back
        data = np.random.randint(0, 256, (200, 200, 3), dtype=np.uint8)
        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 200, 200, 3)

        # Apply crop and downsample
        processed = img.crop((20, 20), (160, 160)).downsample(2)

        output_path = os.path.join(temp_dir, "roundtrip_ops.png")
        processed.write_spng(output_path)

        # Read back and verify dimensions
        img_read = fim.Image.from_png(output_path)
        data_read = img_read.to_numpy()

        # Cropped 160x160, downsampled by 2 = 80x80
        assert data_read.shape == (80, 80, 3)

        # Get the expected data by applying same operations and converting
        expected = processed.to_numpy()
        np.testing.assert_array_equal(data_read, expected)

    def test_roundtrip_checkerboard_pattern(self, temp_dir):
        """Test roundtrip with a checkerboard pattern (easy to verify visually)."""
        # Create a simple checkerboard
        data = np.zeros((64, 64, 3), dtype=np.uint8)
        for y in range(64):
            for x in range(64):
                if (x // 8 + y // 8) % 2 == 0:
                    data[y, x, :] = 255  # White
                else:
                    data[y, x, :] = 0  # Black

        flat_data = data.flatten().tolist()
        img = fim.Image.from_memory(flat_data, 64, 64, 3)
        output_path = os.path.join(temp_dir, "roundtrip_checkerboard.png")
        img.write_spng(output_path)

        # Read back and verify
        img_read = fim.Image.from_png(output_path)
        data_read = img_read.to_numpy()

        np.testing.assert_array_equal(data_read, data)

        # Spot check a few squares
        assert np.all(data_read[0:8, 0:8, :] == 255)  # Top-left white
        assert np.all(data_read[0:8, 8:16, :] == 0)  # Next to it black
        assert np.all(data_read[8:16, 0:8, :] == 0)  # Below it black


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

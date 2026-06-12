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
Tests for Python stage handle validation.

This module tests that invalid PyStage operations properly raise Python
exceptions instead of causing segfaults.
"""

import pytest
import numpy as np

try:
    import fim
except ImportError:
    pytest.skip("fim module not available", allow_module_level=True)


class TestPyStageValidation:
    """Tests for PyStage validation."""

    def test_empty_stage_raises_exception(self):
        """Test that an empty PyStage raises an exception when used."""
        # Create an empty stage
        empty_stage = fim.Image()

        # Attempting to get dimensions should raise an exception
        with pytest.raises(RuntimeError, match="Invalid PyStage|stage is null"):
            empty_stage.dimensions

    def test_empty_stage_crop_raises_exception(self):
        """Test that cropping an empty stage raises an exception."""
        empty_stage = fim.Image()

        # Attempting to crop should raise an exception
        with pytest.raises(RuntimeError, match="Invalid PyStage|stage is null"):
            empty_stage.crop((0, 0), (10, 10))

    def test_empty_stage_downsample_raises_exception(self):
        """Test that downsampling an empty stage raises an exception."""
        empty_stage = fim.Image()

        # Attempting to downsample should raise an exception
        with pytest.raises(RuntimeError, match="Invalid PyStage|stage is null"):
            empty_stage.downsample(2)

    def test_empty_stage_get_tile_raises_exception(self):
        """Test that getting a tile from an empty stage raises an exception."""
        empty_stage = fim.Image()

        # Attempting to get a tile should raise an exception
        with pytest.raises(RuntimeError, match="Invalid PyStage|stage is null"):
            empty_stage.get_tile(0, 0, 10, 10)

    def test_valid_stage_operations_work(self):
        """Test that valid stages work correctly after the validation changes."""
        # Create a valid image
        data = np.ones((100, 100, 3), dtype=np.uint8) * 128
        img = fim.Image.from_numpy(data)

        # All operations should work without exceptions
        width, height, channels = img.dimensions
        assert width == 100
        assert height == 100
        assert channels == 3

        # Crop should work
        cropped = img.crop((10, 10), (50, 50))
        crop_w, crop_h, crop_c = cropped.dimensions
        assert crop_w == 50
        assert crop_h == 50

        # Downsample should work
        downsampled = img.downsample(2)
        down_w, down_h, down_c = downsampled.dimensions
        assert down_w == 50
        assert down_h == 50

        # Get tile should work
        tile = img.get_tile(0, 0, 10, 10)
        assert tile.shape == (10, 10, 3)

    def test_chained_operations_with_validation(self):
        """Test that chained operations work correctly with validation."""
        # Create a valid image
        data = np.ones((200, 200, 3), dtype=np.uint8) * 200
        img = fim.Image.from_numpy(data)

        # Chain multiple operations
        result = img.crop((20, 20), (160, 160)).downsample(2).crop((10, 10), (60, 60))

        # Verify the result is valid
        width, height, channels = result.dimensions
        assert width == 60
        assert height == 60
        assert channels == 3

    def test_error_message_is_descriptive(self):
        """Test that error messages are descriptive and helpful."""
        empty_stage = fim.Image()

        # The error message should mention that the stage is invalid
        with pytest.raises(RuntimeError) as exc_info:
            empty_stage.dimensions

        error_message = str(exc_info.value).lower()
        assert "invalid" in error_message or "null" in error_message or "stage" in error_message

    def test_operations_on_moved_stages(self):
        """Test that operations work correctly after stages are moved/copied."""
        # Create an image
        data = np.ones((50, 50, 1), dtype=np.uint8) * 100
        img = fim.Image.from_numpy(data)

        # Create a copy
        img_copy = img

        # Both should work
        width1, height1, channels1 = img.dimensions
        width2, height2, channels2 = img_copy.dimensions

        assert width1 == width2
        assert height1 == height2

    def test_stack_with_empty_stages_raises(self):
        """Test that stacking with empty stages raises an exception."""
        # Create one valid and one empty stage
        data = np.ones((50, 50, 1), dtype=np.uint8) * 100
        valid_img = fim.Image.from_numpy(data)
        empty_img = fim.Image()

        # Attempting to stack should raise an exception
        with pytest.raises(RuntimeError):
            fim.Image.stack([valid_img, empty_img], axis="bands")

    def test_multiple_operations_without_segfault(self):
        """Test that multiple operations don't cause segfaults with validation."""
        # This test ensures that validation prevents segfaults
        # even in more complex scenarios

        for _ in range(10):
            # Create and immediately use a temporary image
            data = np.random.randint(0, 256, (64, 64, 3), dtype=np.uint8)
            img = fim.Image.from_numpy(data)

            # Chain operations
            result = img.crop((5, 5), (54, 54)).downsample(2)

            # Access the result
            width, height, channels = result.dimensions
            assert width == 27
            assert height == 27

            # Get a tile to ensure data is accessible
            tile = result.get_tile(0, 0, 5, 5)
            assert tile.shape[0] == 5
            assert tile.shape[1] == 5


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

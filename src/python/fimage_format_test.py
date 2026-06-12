#!/usr/bin/env python3
"""Tests for FImage Python bindings."""

import os
import tempfile
import pytest
import numpy as np

import fim


class TestFImageFormat:
    """Test suite for FImage format reading and writing."""

    @pytest.fixture
    def test_image(self):
        """Create a test image (100x100x3)."""
        np.random.seed(42)
        return np.random.randint(0, 256, (100, 100, 3), dtype=np.uint8)

    @pytest.fixture
    def temp_file(self):
        """Create a temporary file path."""
        fd, path = tempfile.mkstemp(suffix=".fimage")
        os.close(fd)
        yield path
        # Cleanup
        if os.path.exists(path):
            os.remove(path)

    def test_compression_enum_accessible(self):
        """Test that CompressionType enum is accessible."""
        assert hasattr(fim, "CompressionType")
        assert hasattr(fim.CompressionType, "NONE")
        assert hasattr(fim.CompressionType, "LZ4")
        assert hasattr(fim.CompressionType, "ZSTD")

        # Check values (use int() for enum-to-int comparison)
        assert int(fim.CompressionType.NONE) == 0
        assert int(fim.CompressionType.LZ4) == 1
        assert int(fim.CompressionType.ZSTD) == 2

    def test_uncompressed_roundtrip(self, test_image, temp_file):
        """Test writing and reading an uncompressed FImage file."""
        # Create image from numpy
        img = fim.Image.from_numpy(test_image)

        # Write uncompressed
        img.write_fimage(temp_file, compression=0)

        # Read back
        img_read = fim.Image.from_fimage(temp_file)
        result = img_read.to_numpy()

        # Verify
        np.testing.assert_array_equal(test_image, result)

    def test_lz4_compression_roundtrip(self, test_image, temp_file):
        """Test writing and reading an LZ4 compressed FImage file."""
        img = fim.Image.from_numpy(test_image)

        # Write with LZ4
        img.write_fimage(temp_file, compression=1)

        # Read back
        img_read = fim.Image.from_fimage(temp_file)
        result = img_read.to_numpy()

        # Verify
        np.testing.assert_array_equal(test_image, result)

    def test_zstd_compression_roundtrip(self, test_image, temp_file):
        """Test writing and reading a Zstd compressed FImage file."""
        img = fim.Image.from_numpy(test_image)

        # Write with Zstd
        img.write_fimage(temp_file, compression=2)

        # Read back
        img_read = fim.Image.from_fimage(temp_file)
        result = img_read.to_numpy()

        # Verify
        np.testing.assert_array_equal(test_image, result)

    def test_tiled_mode(self, test_image, temp_file):
        """Test tiled mode with LZ4 compression."""
        img = fim.Image.from_numpy(test_image)

        # Write with tiling
        img.write_fimage(temp_file, compression=1, tile_width=32, tile_height=32)

        # Read back
        img_read = fim.Image.from_fimage(temp_file)
        result = img_read.to_numpy()

        # Verify
        np.testing.assert_array_equal(test_image, result)

    def test_mpp_metadata(self, test_image, temp_file):
        """Test writing and reading with MPP metadata."""
        img = fim.Image.from_numpy(test_image)

        # Write with MPP metadata
        img.write_fimage(temp_file, compression=0, mpp_x=0.5, mpp_y=0.5)

        # Read back
        img_read = fim.Image.from_fimage(temp_file)
        result = img_read.to_numpy()

        # Verify data
        np.testing.assert_array_equal(test_image, result)

    def test_compression_ratio(self, temp_file):
        """Test that compression actually reduces file size."""
        # Create highly compressible image (solid colors)
        compressible = np.zeros((200, 200, 3), dtype=np.uint8)
        compressible[:100, :, 0] = 255  # Red top half
        compressible[100:, :, 2] = 255  # Blue bottom half

        img = fim.Image.from_numpy(compressible)

        # Write uncompressed
        temp_uncompressed = temp_file + ".uncompressed"
        img.write_fimage(temp_uncompressed, compression=0)
        size_uncompressed = os.path.getsize(temp_uncompressed)

        # Write LZ4 compressed
        temp_lz4 = temp_file + ".lz4"
        img.write_fimage(temp_lz4, compression=1)
        size_lz4 = os.path.getsize(temp_lz4)

        # Write Zstd compressed
        temp_zstd = temp_file + ".zstd"
        img.write_fimage(temp_zstd, compression=2)
        size_zstd = os.path.getsize(temp_zstd)

        # Verify compression reduces size
        assert size_lz4 < size_uncompressed
        assert size_zstd < size_uncompressed

        # Cleanup
        os.remove(temp_uncompressed)
        os.remove(temp_lz4)
        os.remove(temp_zstd)

    def test_lazy_pipeline_with_fimage(self, test_image, temp_file):
        """Test lazy pipeline operations with FImage source."""
        img = fim.Image.from_numpy(test_image)

        # Write tiled FImage
        img.write_fimage(temp_file, compression=1, tile_width=64, tile_height=64)

        # Build lazy pipeline
        pipeline = fim.Image.from_fimage(temp_file).crop((25, 25), (50, 50)).resize(100, 100)

        # Execute pipeline
        result = pipeline.to_numpy()

        # Verify shape
        assert result.shape == (100, 100, 3)

    def test_crop_from_fimage(self, test_image, temp_file):
        """Test cropping directly from FImage without loading entire image."""
        img = fim.Image.from_numpy(test_image)

        # Write FImage
        img.write_fimage(temp_file, compression=0)

        # Load and crop
        img_read = fim.Image.from_fimage(temp_file)
        cropped = img_read.crop((10, 10), (50, 50)).to_numpy()

        # Verify crop
        expected_crop = test_image[10:60, 10:60, :]
        np.testing.assert_array_equal(expected_crop, cropped)

    def test_different_tile_sizes(self, test_image, temp_file):
        """Test various tile sizes."""
        img = fim.Image.from_numpy(test_image)

        tile_sizes = [(16, 16), (32, 32), (64, 64), (128, 128)]

        for tile_w, tile_h in tile_sizes:
            img.write_fimage(temp_file, compression=1, tile_width=tile_w, tile_height=tile_h)

            # Read back
            img_read = fim.Image.from_fimage(temp_file)
            result = img_read.to_numpy()

            # Verify
            np.testing.assert_array_equal(test_image, result, err_msg=f"Failed for tile size {tile_w}x{tile_h}")

    def test_single_channel_image(self, temp_file):
        """Test FImage with single channel (grayscale)."""
        # Create grayscale image
        gray = np.random.randint(0, 256, (100, 100, 1), dtype=np.uint8)

        img = fim.Image.from_numpy(gray)
        img.write_fimage(temp_file, compression=1)

        # Read back
        img_read = fim.Image.from_fimage(temp_file)
        result = img_read.to_numpy()

        # Verify
        np.testing.assert_array_equal(gray, result)

    def test_four_channel_image(self, temp_file):
        """Test FImage with four channels (RGBA)."""
        # Create RGBA image
        rgba = np.random.randint(0, 256, (100, 100, 4), dtype=np.uint8)

        img = fim.Image.from_numpy(rgba)
        img.write_fimage(temp_file, compression=1)

        # Read back
        img_read = fim.Image.from_fimage(temp_file)
        result = img_read.to_numpy()

        # Verify
        np.testing.assert_array_equal(rgba, result)

    def test_large_image_tiled(self, temp_file):
        """Test large image with tiling."""
        # Create larger test image
        large = np.random.randint(0, 256, (512, 512, 3), dtype=np.uint8)

        img = fim.Image.from_numpy(large)
        img.write_fimage(temp_file, compression=2, tile_width=128, tile_height=128)

        # Read back
        img_read = fim.Image.from_fimage(temp_file)
        result = img_read.to_numpy()

        # Verify
        np.testing.assert_array_equal(large, result)

    def test_get_dimensions_from_fimage(self, test_image, temp_file):
        """Test getting dimensions from FImage source."""
        img = fim.Image.from_numpy(test_image)
        img.write_fimage(temp_file, compression=0)

        # Read and check dimensions
        img_read = fim.Image.from_fimage(temp_file)
        width, height, channels = img_read.dimensions

        assert width == 100
        assert height == 100
        assert channels == 3

    def test_get_tile_from_fimage(self, test_image, temp_file):
        """Test getting a tile directly from FImage."""
        img = fim.Image.from_numpy(test_image)
        img.write_fimage(temp_file, compression=1, tile_width=64, tile_height=64)

        # Read FImage and get a tile
        img_read = fim.Image.from_fimage(temp_file)
        tile = img_read.get_tile(20, 20, 30, 30)

        # Verify tile shape
        assert tile.shape == (30, 30, 3)

        # Verify tile content
        expected = test_image[20:50, 20:50, :]
        np.testing.assert_array_equal(expected, tile)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

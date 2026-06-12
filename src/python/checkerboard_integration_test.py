#!/usr/bin/env python3
"""
Comprehensive checkerboard test for fim Python bindings.

This test module creates synthetic checkerboard patterns and validates
complex processing pipelines, similar to the C++ checkerboard_demo.
"""

import os
import tempfile

import numpy as np
import pytest

import fim


def generate_checkerboard(width, height, channels, square_size=100):
    """Generate a checkerboard pattern in memory."""
    data = np.zeros((height, width, channels), dtype=np.uint8)

    for y in range(height):
        for x in range(width):
            square_x = x // square_size
            square_y = y // square_size
            is_white = (square_x + square_y) % 2 == 0
            value = 255 if is_white else 0
            data[y, x, :] = value

    return data


@pytest.fixture
def temp_dir():
    """Create a temporary directory for test outputs."""
    tmp = tempfile.mkdtemp()
    yield tmp
    import shutil

    if os.path.exists(tmp):
        shutil.rmtree(tmp)


def test_generate_checkerboard():
    """Test checkerboard pattern generation."""
    board = generate_checkerboard(400, 400, 3, square_size=50)

    assert board.shape == (400, 400, 3)
    assert board.dtype == np.uint8

    # Verify checkerboard pattern
    # Top-left square should be white (even + even = 0 = white)
    assert board[0, 0, 0] == 255

    # One square to the right should be black
    assert board[0, 50, 0] == 0

    # One square down should be black
    assert board[50, 0, 0] == 0

    # Diagonal square should be white
    assert board[50, 50, 0] == 255


def test_checkerboard_to_memory_pipeline():
    """Test memory-to-memory pipeline with checkerboard."""
    # Create a checkerboard where downsample regions will cross square boundaries
    # Use 51-pixel squares (odd size) with 2x downsample to ensure mixing
    board = generate_checkerboard(1000, 1000, 3, square_size=51)

    # Create fim image from memory
    flat_data = board.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 1000, 1000, 3)

    # Apply operations: crop region, then downsample
    result = img.crop((50, 50), (500, 500)).downsample(2)

    # Verify dimensions
    dims = result.dimensions
    assert dims[0] == 250  # 500 / 2
    assert dims[1] == 250  # 500 / 2
    assert dims[2] == 3

    # Convert to numpy and verify
    output = result.to_numpy()
    assert output.shape == (250, 250, 3)

    # Downsampled checkerboard should have values at 0, 127-128, and 255
    # (from averaging black and white squares at boundaries)
    unique_vals = np.unique(output)
    assert 0 in unique_vals
    assert 255 in unique_vals
    # Should have intermediate values from averaging at square boundaries
    assert any(100 < v < 200 for v in unique_vals)


def test_checkerboard_to_tiff(temp_dir):
    """Test writing checkerboard to TIFF file."""
    # Create checkerboard
    board = generate_checkerboard(512, 512, 3, square_size=64)

    flat_data = board.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 512, 512, 3)

    # Write to TIFF
    output_path = os.path.join(temp_dir, "checkerboard_test.tiff")
    img.write_tiff(output_path)

    # Verify file exists and has size
    assert os.path.exists(output_path)
    assert os.path.getsize(output_path) > 0


def test_checkerboard_crop_and_downsample():
    """Test complex pipeline: crop then downsample."""
    # Create 2048x2048 checkerboard
    board = generate_checkerboard(2048, 2048, 3, square_size=128)

    flat_data = board.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 2048, 2048, 3)

    # Crop to center 1024x1024, then downsample by 4
    result = img.crop((512, 512), (1024, 1024)).downsample(4)

    dims = result.dimensions
    assert dims[0] == 256  # 1024 / 4
    assert dims[1] == 256  # 1024 / 4

    # Get actual output
    output = result.to_numpy()
    assert output.shape == (256, 256, 3)

    # Verify pattern still recognizable
    # After downsampling, we should still see variation
    std_dev = np.std(output[:, :, 0])
    assert std_dev > 50  # Should have significant variation


def test_large_checkerboard_memory_efficiency():
    """Test that large images can be processed efficiently."""
    # Create a larger checkerboard (but not too large for tests)
    board = generate_checkerboard(4096, 4096, 3, square_size=256)

    flat_data = board.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 4096, 4096, 3)

    # Process with multiple operations
    result = img.crop((1000, 1000), (2048, 2048)).downsample(2).crop((256, 256), (512, 512))

    dims = result.dimensions
    assert dims[0] == 512
    assert dims[1] == 512

    # Should complete without memory issues
    output = result.to_numpy()
    assert output.shape == (512, 512, 3)


def test_checkerboard_output_verification():
    """Test that checkerboard processing preserves expected patterns."""
    # Create a small, predictable checkerboard
    board = generate_checkerboard(256, 256, 3, square_size=64)

    flat_data = board.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 256, 256, 3)

    # Get the full image back
    output = img.to_numpy()

    # Verify specific squares
    # Square at (0, 0) should be white
    assert np.all(output[0:64, 0:64, :] == 255)

    # Square at (0, 64) should be black
    assert np.all(output[0:64, 64:128, :] == 0)

    # Square at (64, 0) should be black
    assert np.all(output[64:128, 0:64, :] == 0)

    # Square at (64, 64) should be white
    assert np.all(output[64:128, 64:128, :] == 255)


def test_downsample_averaging():
    """Test that downsampling properly averages checkerboard squares."""
    # Create a checkerboard where 2x2 downsample regions will cross square boundaries
    # Use 3-pixel squares so that 2x2 regions will span multiple squares
    board = generate_checkerboard(10, 10, 1, square_size=3)

    flat_data = board.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 10, 10, 1)

    # Downsample by 2 (10x10 -> 5x5)
    result = img.downsample(2)
    output = result.to_numpy()

    assert output.shape == (5, 5, 1)

    # Top-left (0,0) averages pixels (0-1, 0-1) which are all white -> 255
    assert output[0, 0, 0] == 255

    # Position (0,1) averages pixels (2-3, 0-1) which mix white/black -> ~127
    assert 120 <= output[0, 1, 0] <= 135

    # Position (1,1) averages pixels (2-3, 2-3) which mix all corners -> ~127
    assert 120 <= output[1, 1, 0] <= 135


def test_multi_stage_pipeline():
    """Test a complex multi-stage processing pipeline."""
    # Create gradient image
    data = np.zeros((512, 512, 3), dtype=np.uint8)
    for y in range(512):
        for x in range(512):
            data[y, x, 0] = (x * 255) // 512  # Red gradient
            data[y, x, 1] = (y * 255) // 512  # Green gradient
            data[y, x, 2] = 128  # Constant blue

    flat_data = data.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 512, 512, 3)

    # Complex pipeline: crop -> downsample -> crop -> downsample
    result = (
        img.crop((100, 100), (312, 312))  # 312x312
        .downsample(2)  # 156x156
        .crop((28, 28), (100, 100))  # 100x100
        .downsample(2)
    )  # 50x50

    output = result.to_numpy()
    assert output.shape == (50, 50, 3)

    # Verify gradients are still present
    assert np.std(output[:, :, 0]) > 10  # Red varies
    assert np.std(output[:, :, 1]) > 10  # Green varies
    assert np.std(output[:, :, 2]) < 5  # Blue constant


def test_pipeline_with_file_output(temp_dir):
    """Test pipeline that writes to file and verify output."""
    # Create test image
    board = generate_checkerboard(1024, 1024, 3, square_size=128)

    flat_data = board.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 1024, 1024, 3)

    # Process and write to PNG
    png_path = os.path.join(temp_dir, "pipeline_output.png")
    pipeline = img.crop((256, 256), (512, 512)).downsample(2)
    pipeline.write_png(png_path)

    # Verify file exists and can be read back
    assert os.path.exists(png_path)
    assert os.path.getsize(png_path) > 0
    # Verify roundtrip works correctly
    img_read = fim.Image.from_png(png_path)
    assert img_read.dimensions == (256, 256, 3)

    # Process and write to TIFF
    tiff_path = os.path.join(temp_dir, "pipeline_output.tiff")
    img.crop((256, 256), (512, 512)).downsample(2).write_tiff(tiff_path)

    # Verify file exists and has content
    assert os.path.exists(tiff_path)
    assert os.path.getsize(tiff_path) > 0


def test_branching_pipeline():
    """Test that we can branch from the same source."""
    # Create source
    board = generate_checkerboard(512, 512, 3, square_size=64)
    flat_data = board.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 512, 512, 3)

    # Create multiple branches
    branch1 = img.crop((0, 0), (256, 256)).downsample(2)
    branch2 = img.crop((256, 256), (256, 256)).downsample(4)
    branch3 = img.downsample(2)

    # Evaluate all branches
    out1 = branch1.to_numpy()
    out2 = branch2.to_numpy()
    out3 = branch3.to_numpy()

    assert out1.shape == (128, 128, 3)
    assert out2.shape == (64, 64, 3)
    assert out3.shape == (256, 256, 3)

#!/usr/bin/env python3
"""
Tests for fim Python bindings.

This test module validates the lazy evaluation pipeline, stacking operations,
NumPy conversion, and overall API functionality using pytest.
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


def test_image_from_memory():
    """Test creating an image from memory."""
    # Create a simple 2x2 RGB image
    data = np.array([[[255, 0, 0], [0, 255, 0]], [[0, 0, 255], [255, 255, 0]]], dtype=np.uint8)

    flat_data = data.flatten().tolist()
    img = fim.Image.from_memory(flat_data, 2, 2, 3)

    assert img.is_valid()
    dims = img.dimensions
    assert dims == (2, 2, 3)


def test_lazy_evaluation():
    """Test that operations are lazy until sink is called."""
    # Create a test image
    data = [255] * (10 * 10 * 3)
    img = fim.Image.from_memory(data, 10, 10, 3)

    # Apply operations - these should be lazy
    cropped = img.crop((0, 0), (5, 5))
    downsampled = cropped.downsample(2)

    # Verify that the pipeline is constructed correctly
    assert downsampled.is_valid()
    dims = downsampled.dimensions
    # Downsampled 5x5 by factor 2 should give ~2-3
    assert dims[0] <= 3
    assert dims[1] <= 3


def test_crop_operation():
    """Test crop operation."""
    # Create a 10x10 image with all white pixels
    data = [255] * (10 * 10 * 3)
    img = fim.Image.from_memory(data, 10, 10, 3)

    # Crop to 5x5
    cropped = img.crop((0, 0), (5, 5))
    dims = cropped.dimensions

    assert dims[0] == 5  # width
    assert dims[1] == 5  # height
    assert dims[2] == 3  # channels


def test_downsample_operation():
    """Test downsample operation."""
    # Create a 10x10 image
    data = [128] * (10 * 10 * 1)
    img = fim.Image.from_memory(data, 10, 10, 1)

    # Downsample by factor 2
    downsampled = img.downsample(2)
    dims = downsampled.dimensions

    # Should be 5x5 after downsampling
    assert dims[0] == 5
    assert dims[1] == 5
    assert dims[2] == 1


def test_numpy_conversion():
    """Test conversion to NumPy array."""
    # Create a known pattern
    data = []
    for y in range(4):
        for x in range(4):
            data.extend([x * 60, y * 60, 128])  # RGB gradient

    img = fim.Image.from_memory(data, 4, 4, 3)
    array = img.to_numpy()

    # Check shape
    assert array.shape == (4, 4, 3)
    assert array.dtype == np.uint8

    # Verify some pixel values
    assert array[0, 0, 0] == 0  # First pixel R
    assert array[0, 0, 1] == 0  # First pixel G
    assert array[0, 0, 2] == 128  # First pixel B


def test_numpy_conversion_preserves_channels_first_layout():
    """Test that to_numpy() preserves CHANNELS_FIRST by default."""
    chw = np.arange(3 * 4 * 5, dtype=np.uint16).reshape(3, 4, 5)
    img = fim.Image.from_numpy(chw, data_layout=fim.DataLayout.CHANNELS_FIRST)

    out = img.to_numpy()
    assert out.shape == (3, 4, 5)
    assert out.dtype == np.uint16
    np.testing.assert_array_equal(out, chw)

    # Override output layout explicitly
    out_hwc = img.to_numpy(data_layout=fim.DataLayout.CHANNELS_LAST)
    assert out_hwc.shape == (4, 5, 3)
    assert out_hwc.dtype == np.uint16


def test_from_numpy_requires_contiguous():
    """Test that from_numpy() errors on non-contiguous arrays."""
    hwc = np.arange(4 * 5 * 3, dtype=np.uint8).reshape(4, 5, 3)
    non_contig = hwc.transpose(1, 0, 2)  # not C-contiguous
    assert not non_contig.flags["C_CONTIGUOUS"]

    with pytest.raises(Exception, match="C-contiguous"):
        _ = fim.Image.from_numpy(non_contig)


def test_stack_operation():
    """Test stacking multiple images."""
    # Create three single-channel images (simulating R, G, B)
    red_data = [255, 0, 0, 255] * 1  # 2x2 red channel
    green_data = [0, 255, 0, 255] * 1  # 2x2 green channel
    blue_data = [0, 0, 255, 255] * 1  # 2x2 blue channel

    red = fim.Image.from_memory(red_data, 2, 2, 1)
    green = fim.Image.from_memory(green_data, 2, 2, 1)
    blue = fim.Image.from_memory(blue_data, 2, 2, 1)

    # Stack them
    rgb = fim.Image.stack([red, green, blue], axis="bands")

    # Verify dimensions
    dims = rgb.dimensions
    assert dims[0] == 2  # width
    assert dims[1] == 2  # height
    assert dims[2] == 3  # channels (1+1+1)

    # Convert to numpy and verify channel stacking
    array = rgb.to_numpy()
    assert array.shape == (2, 2, 3)


def test_write_png(temp_dir):
    """Test writing to PNG file."""
    # Create a simple image
    data = [255, 0, 0] * (10 * 10)  # Red image
    img = fim.Image.from_memory(data, 10, 10, 3)

    output_path = os.path.join(temp_dir, "test_output.png")
    img.write_png(output_path)

    # Verify file was created
    assert os.path.exists(output_path)
    assert os.path.getsize(output_path) > 0


def test_write_tiff(temp_dir):
    """Test writing to TIFF file."""
    # Create a simple image
    data = [0, 255, 0] * (10 * 10)  # Green image
    img = fim.Image.from_memory(data, 10, 10, 3)

    output_path = os.path.join(temp_dir, "test_output.tiff")
    img.write_tiff(output_path)

    # Verify file was created
    assert os.path.exists(output_path)
    assert os.path.getsize(output_path) > 0


def test_chained_operations():
    """Test method chaining."""
    # Create an image and chain multiple operations
    data = [128] * (20 * 20 * 3)
    img = fim.Image.from_memory(data, 20, 20, 3)

    # Chain operations
    result = img.crop((5, 5), (10, 10)).downsample(2)

    # Verify final dimensions
    dims = result.dimensions
    assert dims[0] == 5  # 10/2
    assert dims[1] == 5  # 10/2
    assert dims[2] == 3


def test_invalid_image():
    """Test that invalid images are handled properly."""
    img = fim.Image()  # Empty/invalid image
    assert not img.is_valid()


def test_repr():
    """Test string representation."""
    data = [0] * (5 * 5 * 1)
    img = fim.Image.from_memory(data, 5, 5, 1)

    repr_str = repr(img)
    assert "Image" in repr_str
    assert "5" in repr_str  # Width and height
    assert "1" in repr_str  # Channels


def test_stack_incompatible_dimensions():
    """Test that stacking images with different dimensions fails."""
    img1 = fim.Image.from_memory([0] * (5 * 5 * 1), 5, 5, 1)
    img2 = fim.Image.from_memory([0] * (10 * 10 * 1), 10, 10, 1)

    # This should raise an error
    with pytest.raises(RuntimeError):
        fim.Image.stack([img1, img2], axis="bands")


def test_numpy_roundtrip():
    """Test that converting to numpy and back preserves data."""
    # Create a pattern with known values
    width, height, channels = 8, 8, 3
    data = []
    for y in range(height):
        for x in range(width):
            # Create a gradient pattern
            r = (x * 255) // width
            g = (y * 255) // height
            b = 128
            data.extend([r, g, b])

    # Create image, convert to numpy
    img = fim.Image.from_memory(data, width, height, channels)
    array = img.to_numpy()

    # Convert back to fim
    flat_data = array.flatten().tolist()
    img2 = fim.Image.from_memory(flat_data, width, height, channels)
    array2 = img2.to_numpy()

    # Should be identical
    np.testing.assert_array_equal(array, array2)


def test_numpy_channels_first_roundtrip():
    """Test CHW (CHANNELS_FIRST) numpy import/export and conversions."""
    channels, height, width = 3, 6, 5
    chw = np.zeros((channels, height, width), dtype=np.uint8)
    chw[0, :, :] = 10  # R plane
    chw[1, :, :] = 20  # G plane
    chw[2, :, :] = 30  # B plane
    chw[0, 2, 3] = 123

    img = fim.Image.from_numpy(chw, data_layout=fim.DataLayout.CHANNELS_FIRST)

    out_chw = img.to_numpy(data_layout=fim.DataLayout.CHANNELS_FIRST)
    assert out_chw.shape == (channels, height, width)
    np.testing.assert_array_equal(out_chw, chw)

    out_hwc = img.to_numpy(data_layout=fim.DataLayout.CHANNELS_LAST)
    assert out_hwc.shape == (height, width, channels)
    assert out_hwc[2, 3, 0] == 123
    assert out_hwc[0, 0, 1] == 20


def test_to_layout_is_lazy_and_correct():
    """Test that to_layout() returns a new image with converted layout."""
    height, width, channels = 4, 3, 2
    hwc = np.arange(height * width * channels, dtype=np.uint8).reshape(height, width, channels)

    img = fim.Image.from_numpy(hwc)  # default: CHANNELS_LAST
    img_chw = img.to_layout(fim.DataLayout.CHANNELS_FIRST)

    out_chw = img_chw.to_numpy(data_layout=fim.DataLayout.CHANNELS_FIRST)
    assert out_chw.shape == (channels, height, width)
    # spot-check: channel 0 plane matches
    np.testing.assert_array_equal(out_chw[0], hwc[:, :, 0])


def test_to_numpy_array_outlives_image():
    """Buffer returned by to_numpy() must keep its data alive after the source
    image is garbage collected.

    This validates the nanobind capsule ownership: the numpy array carries a
    capsule that owns the underlying std::vector, so dropping the source
    Image must not invalidate the buffer.
    """
    import gc

    height, width, channels = 7, 11, 3
    src = np.arange(height * width * channels, dtype=np.uint8).reshape(height, width, channels)
    img = fim.Image.from_numpy(src)
    array = img.to_numpy()
    assert array.shape == (height, width, channels)

    del img
    gc.collect()

    np.testing.assert_array_equal(array, src)


def test_to_numpy_returns_writable_buffer():
    """to_numpy() must return a writable numpy array (not a read-only view)."""
    src = np.full((4, 5, 3), 17, dtype=np.uint8)
    img = fim.Image.from_numpy(src)
    array = img.to_numpy()

    assert array.flags.writeable
    array[0, 0, 0] = 99
    assert array[0, 0, 0] == 99


def test_get_tile_returns_correct_region():
    """get_tile() returns the requested region as a numpy array."""
    height, width, channels = 16, 16, 3
    src = np.arange(height * width * channels, dtype=np.uint8).reshape(height, width, channels)
    img = fim.Image.from_numpy(src)

    tile = img.get_tile(2, 3, 5, 4)
    assert tile.shape == (4, 5, channels)
    np.testing.assert_array_equal(tile, src[3:7, 2:7, :])


def test_box_class():
    """Box has accessible coordinates and width/height accessors."""
    box = fim.Box(1.5, 2.5, 4.5, 6.5)
    assert box.x1 == 1.5
    assert box.y1 == 2.5
    assert box.x2 == 4.5
    assert box.y2 == 6.5
    assert box.width() == 3.0
    assert box.height() == 4.0
    assert "Box" in repr(box)


def test_resize_with_kernel_types():
    """resize() accepts the supported KernelType enum values."""
    src = np.zeros((16, 16, 3), dtype=np.uint8)
    img = fim.Image.from_numpy(src)
    for kernel in (
        fim.KernelType.NEAREST,
        fim.KernelType.LANCZOS2,
        fim.KernelType.LANCZOS3,
        fim.KernelType.MAGIC2021,
    ):
        out = img.resize(8, 8, kernel=kernel).to_numpy()
        assert out.shape == (8, 8, 3)


def test_resize_zero_dimension_input():
    """Test that resizing an image with zero width or height raises ValueError.

    This test reproduces the crash scenario where a crop produces a zero-height
    image and resize is called on it. The resize operation should now raise a
    clear ValueError instead of crashing the interpreter.
    """
    # Create a valid image
    data = [128] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    # Crop that results in zero height (out of bounds crop)
    cropped = img.crop((0, 200), (50, 50))  # y=200 is beyond height=100

    # Verify the crop has zero height
    dims = cropped.dimensions
    assert dims[1] == 0, f"Expected zero height, got {dims[1]}"

    # Attempting to resize should raise ValueError with informative message
    with pytest.raises(ValueError) as exc_info:
        resized = cropped.resize(25, 25, fim.KernelType.LANCZOS3)

    # Verify the error message is informative
    error_msg = str(exc_info.value)
    assert "input dimensions must be positive" in error_msg.lower()

    # Also test zero width scenario
    cropped_zero_width = img.crop((200, 0), (50, 50))  # x=200 is beyond width=100
    dims_zero_width = cropped_zero_width.dimensions
    assert dims_zero_width[0] == 0, f"Expected zero width, got {dims_zero_width[0]}"

    with pytest.raises(ValueError) as exc_info2:
        resized = cropped_zero_width.resize(25, 25, fim.KernelType.LANCZOS3)

    error_msg2 = str(exc_info2.value)
    assert "input dimensions must be positive" in error_msg2.lower()

#!/usr/bin/env python3
"""
Tests for fim resize operator Python bindings.

This test module validates the resize functionality with different kernels,
box parameters, and integration with the lazy evaluation pipeline.
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


def test_resize_basic_downsample():
    """Test basic resize operation with downsampling."""
    # Create a 100x100 image
    data = [128] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    # Resize to 50x50
    resized = img.resize(50, 50)
    dims = resized.dimensions

    assert dims[0] == 50  # width
    assert dims[1] == 50  # height
    assert dims[2] == 3  # channels


def test_resize_basic_upsample():
    """Test basic resize operation with upsampling."""
    # Create a 50x50 image
    data = [100] * (50 * 50 * 1)
    img = fim.Image.from_memory(data, 50, 50, 1)

    # Resize to 100x100
    resized = img.resize(100, 100)
    dims = resized.dimensions

    assert dims[0] == 100  # width
    assert dims[1] == 100  # height
    assert dims[2] == 1  # channels


def test_resize_non_uniform():
    """Test resize with non-uniform scaling."""
    data = [200] * (100 * 50 * 3)
    img = fim.Image.from_memory(data, 100, 50, 3)

    # Resize to 200x100
    resized = img.resize(200, 100)
    dims = resized.dimensions

    assert dims[0] == 200
    assert dims[1] == 100
    assert dims[2] == 3


def test_resize_with_lanczos2():
    """Test resize with Lanczos2 kernel."""
    data = [150] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    resized = img.resize(50, 50, kernel=fim.KernelType.LANCZOS2)
    dims = resized.dimensions

    assert dims[0] == 50
    assert dims[1] == 50


def test_resize_with_lanczos3():
    """Test resize with Lanczos3 kernel (default)."""
    data = [150] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    resized = img.resize(50, 50, kernel=fim.KernelType.LANCZOS3)
    dims = resized.dimensions

    assert dims[0] == 50
    assert dims[1] == 50


def test_resize_with_magic2021():
    """Test resize with Magic2021 kernel."""
    data = [180] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    resized = img.resize(50, 50, kernel=fim.KernelType.MAGIC2021)
    dims = resized.dimensions

    assert dims[0] == 50
    assert dims[1] == 50


def test_resize_with_box_parameter():
    """Test resize with box parameter for subpixel cropping."""
    data = [255] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    # Create a box for subpixel region
    box = fim.Box(10.5, 20.5, 60.5, 70.5)
    resized = img.resize(50, 50, box=box)

    dims = resized.dimensions
    assert dims[0] == 50
    assert dims[1] == 50


def test_box_class():
    """Test Box class functionality."""
    box = fim.Box(10.0, 20.0, 110.0, 120.0)

    assert box.x1 == 10.0
    assert box.y1 == 20.0
    assert box.x2 == 110.0
    assert box.y2 == 120.0
    assert box.width() == 100.0
    assert box.height() == 100.0


def test_box_repr():
    """Test Box string representation."""
    box = fim.Box(5.5, 10.5, 55.5, 60.5)
    repr_str = repr(box)

    assert "Box" in repr_str
    assert "5.5" in repr_str
    assert "10.5" in repr_str


def test_resize_lazy_evaluation():
    """Test that resize is lazy until sink is called."""
    data = [100] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    # Apply resize - should be lazy
    resized = img.resize(50, 50)

    # Verify pipeline is constructed
    assert resized.is_valid()
    dims = resized.dimensions
    assert dims[0] == 50


def test_resize_chain_with_crop():
    """Test chaining resize with other operations."""
    data = [128] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    # Chain crop then resize
    result = img.crop((10, 10), (80, 80)).resize(40, 40)
    dims = result.dimensions

    assert dims[0] == 40
    assert dims[1] == 40
    assert dims[2] == 3


def test_resize_to_numpy():
    """Test converting resized image to NumPy array."""
    # Create a known pattern
    data = []
    for y in range(50):
        for x in range(50):
            data.extend([x * 5, y * 5, 128])

    img = fim.Image.from_memory(data, 50, 50, 3)
    resized = img.resize(25, 25)
    array = resized.to_numpy()

    # Check shape
    assert array.shape == (25, 25, 3)
    assert array.dtype == np.uint8


def test_resize_to_png(temp_dir):
    """Test writing resized image to PNG file."""
    data = [255, 0, 0] * (100 * 100)  # Red image
    img = fim.Image.from_memory(data, 100, 100, 3)

    resized = img.resize(50, 50)
    output_path = os.path.join(temp_dir, "resized_output.png")
    resized.write_png(output_path)

    # Verify file was created
    assert os.path.exists(output_path)
    assert os.path.getsize(output_path) > 0


def test_resize_different_channels():
    """Test resize with different channel counts."""
    # Grayscale
    gray_data = [100] * (50 * 50 * 1)
    gray_img = fim.Image.from_memory(gray_data, 50, 50, 1)
    gray_resized = gray_img.resize(25, 25)
    gray_dims = gray_resized.dimensions
    assert gray_dims[2] == 1

    # RGBA
    rgba_data = [200] * (50 * 50 * 4)
    rgba_img = fim.Image.from_memory(rgba_data, 50, 50, 4)
    rgba_resized = rgba_img.resize(25, 25)
    rgba_dims = rgba_resized.dimensions
    assert rgba_dims[2] == 4


def test_resize_extreme_downsample():
    """Test extreme downsampling."""
    data = [150] * (1000 * 1000 * 3)
    img = fim.Image.from_memory(data, 1000, 1000, 3)

    # Extreme downsample
    resized = img.resize(50, 50)
    dims = resized.dimensions

    assert dims[0] == 50
    assert dims[1] == 50


def test_resize_extreme_upsample():
    """Test extreme upsampling."""
    data = [50] * (10 * 10 * 3)
    img = fim.Image.from_memory(data, 10, 10, 3)

    # Extreme upsample
    resized = img.resize(100, 100)
    dims = resized.dimensions

    assert dims[0] == 100
    assert dims[1] == 100


def test_kernel_type_enum():
    """Test KernelType enum values."""
    assert hasattr(fim, "KernelType")
    assert hasattr(fim.KernelType, "LANCZOS2")
    assert hasattr(fim.KernelType, "LANCZOS3")
    assert hasattr(fim.KernelType, "MAGIC2021")


def test_resize_preserves_data_range():
    """Test that resize keeps values in valid uint8 range."""
    # Create gradient pattern
    data = []
    for y in range(20):
        for x in range(20):
            val = (x + y) * 6
            data.extend([val, val, val])

    img = fim.Image.from_memory(data, 20, 20, 3)
    resized = img.resize(10, 10)
    array = resized.to_numpy()

    # All values should be in [0, 255]
    assert np.all(array >= 0)
    assert np.all(array <= 255)


def test_resize_multiple_times():
    """Test applying resize multiple times."""
    data = [200] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    # Multiple resizes
    r1 = img.resize(80, 80)
    r2 = r1.resize(60, 60)
    r3 = r2.resize(40, 40)

    dims = r3.dimensions
    assert dims[0] == 40
    assert dims[1] == 40


def test_resize_with_subpixel_box():
    """Test resize with precise subpixel box."""
    data = [255] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    # Subpixel box for precise region selection
    box = fim.Box(0.25, 0.75, 99.25, 99.75)
    resized = img.resize(50, 50, kernel=fim.KernelType.MAGIC2021, box=box)

    array = resized.to_numpy()
    assert array.shape == (50, 50, 3)


def test_resize_get_tile():
    """Test getting tiles from resized image."""
    data = [128] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    resized = img.resize(50, 50)

    # Get a tile
    tile = resized.get_tile(0, 0, 25, 25)
    assert tile.shape == (25, 25, 3)


def test_from_fastslide_method_exists():
    """Test that from_fastslide method exists."""
    assert hasattr(fim.Image, "from_fastslide")

    # Note: We can't test actual loading without a real slide file
    # but we can verify the method signature exists

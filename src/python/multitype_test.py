"""Tests for multi-type support in FImage Python bindings."""

import numpy as np
import pytest
import fim
import tempfile
import os


def test_uint8_round_trip():
    """Test uint8 numpy round-trip."""
    # Create uint8 array
    arr_uint8 = np.array([[[1, 2, 3], [4, 5, 6]], [[7, 8, 9], [10, 11, 12]]], dtype=np.uint8)

    # Convert to fim Image
    img = fim.Image.from_numpy(arr_uint8)

    # Convert back to numpy
    result = img.to_numpy()

    assert result.dtype == np.uint8
    assert result.shape == (2, 2, 3)
    np.testing.assert_array_equal(result, arr_uint8)


def test_uint16_round_trip():
    """Test uint16 numpy round-trip."""
    # Create uint16 array
    arr_uint16 = np.array([[[100, 200, 300], [400, 500, 600]], [[700, 800, 900], [1000, 1100, 1200]]], dtype=np.uint16)

    # Convert to fim Image
    img = fim.Image.from_numpy(arr_uint16)

    # Convert back to numpy
    result = img.to_numpy()

    assert result.dtype == np.uint16
    assert result.shape == (2, 2, 3)
    np.testing.assert_array_equal(result, arr_uint16)


def test_float32_round_trip():
    """Test float32 numpy round-trip."""
    # Create float32 array
    arr_float = np.array([[[0.1, 0.2, 0.3], [0.4, 0.5, 0.6]], [[0.7, 0.8, 0.9], [1.0, 1.1, 1.2]]], dtype=np.float32)

    # Convert to fim Image
    img = fim.Image.from_numpy(arr_float)

    # Convert back to numpy
    result = img.to_numpy()

    assert result.dtype == np.float32
    assert result.shape == (2, 2, 3)
    np.testing.assert_array_almost_equal(result, arr_float)


def test_uint16_fimage_write_read():
    """Test writing and reading uint16 FImage files."""
    # Create uint16 array
    arr_uint16 = np.arange(4 * 3 * 2, dtype=np.uint16).reshape(4, 3, 2) * 1000

    # Convert to fim Image
    img = fim.Image.from_numpy(arr_uint16)

    # Write to FImage file
    with tempfile.NamedTemporaryFile(suffix=".fimage", delete=False) as f:
        filename = f.name

    try:
        img.write_fimage(filename)

        # Read back
        img_read = fim.Image.from_fimage(filename)
        result = img_read.to_numpy()

        assert result.dtype == np.uint16
        assert result.shape == (4, 3, 2)
        np.testing.assert_array_equal(result, arr_uint16)
    finally:
        if os.path.exists(filename):
            os.unlink(filename)


def test_float32_fimage_write_read():
    """Test writing and reading float32 FImage files."""
    # Create float32 array
    arr_float = np.arange(3 * 2 * 1, dtype=np.float32).reshape(3, 2, 1) * 0.5

    # Convert to fim Image
    img = fim.Image.from_numpy(arr_float)

    # Write to FImage file with compression
    with tempfile.NamedTemporaryFile(suffix=".fimage", delete=False) as f:
        filename = f.name

    try:
        # Use no compression for now (compression will be tested separately)
        img.write_fimage(filename, compression=fim.CompressionType.NONE)

        # Read back
        img_read = fim.Image.from_fimage(filename)
        result = img_read.to_numpy()

        assert result.dtype == np.float32
        assert result.shape == (3, 2, 1)
        np.testing.assert_array_almost_equal(result, arr_float)
    finally:
        if os.path.exists(filename):
            os.unlink(filename)


def test_get_tile_with_different_dtypes():
    """Test get_tile returns correct dtype."""
    # Test uint8
    arr_uint8 = np.arange(10 * 8 * 3, dtype=np.uint8).reshape(10, 8, 3)
    img_uint8 = fim.Image.from_numpy(arr_uint8)
    tile_uint8 = img_uint8.get_tile(0, 0, 3, 3)
    assert tile_uint8.dtype == np.uint8
    assert tile_uint8.shape == (3, 3, 3)

    # Test uint16
    arr_uint16 = np.arange(10 * 8 * 3, dtype=np.uint16).reshape(10, 8, 3) * 100
    img_uint16 = fim.Image.from_numpy(arr_uint16)
    tile_uint16 = img_uint16.get_tile(0, 0, 3, 3)
    assert tile_uint16.dtype == np.uint16
    assert tile_uint16.shape == (3, 3, 3)

    # Test float32
    arr_float = np.arange(10 * 8 * 1, dtype=np.float32).reshape(10, 8, 1) * 0.1
    img_float = fim.Image.from_numpy(arr_float)
    tile_float = img_float.get_tile(0, 0, 3, 3)
    assert tile_float.dtype == np.float32
    assert tile_float.shape == (3, 3, 1)


def test_pixel_type_enum_exposed():
    """Test that PixelType enum is exposed to Python."""
    # Check that PixelType enum values are accessible
    assert hasattr(fim, "PixelType")
    assert hasattr(fim.PixelType, "UINT8")
    assert hasattr(fim.PixelType, "UINT16")
    assert hasattr(fim.PixelType, "FLOAT32")

    # Check numeric values (use .value to get int)
    assert int(fim.PixelType.UINT8) == 0
    assert int(fim.PixelType.UINT16) == 1
    assert int(fim.PixelType.FLOAT32) == 2


def test_image_dtype_property():
    """Test that Image has dtype and pixel_type properties."""
    # Test uint8
    arr_uint8 = np.array([[[1, 2], [3, 4]]], dtype=np.uint8)
    img_uint8 = fim.Image.from_numpy(arr_uint8)
    assert img_uint8.dtype == "uint8"
    assert img_uint8.pixel_type == fim.PixelType.UINT8

    # Test uint16
    arr_uint16 = np.array([[[100, 200], [300, 400]]], dtype=np.uint16)
    img_uint16 = fim.Image.from_numpy(arr_uint16)
    assert img_uint16.dtype == "uint16"
    assert img_uint16.pixel_type == fim.PixelType.UINT16

    # Test float32
    arr_float = np.array([[[0.1, 0.2], [0.3, 0.4]]], dtype=np.float32)
    img_float = fim.Image.from_numpy(arr_float)
    assert img_float.dtype == "float32"
    assert img_float.pixel_type == fim.PixelType.FLOAT32


def test_image_repr_includes_dtype():
    """Test that Image.__repr__ includes dtype."""
    # Test uint8
    arr_uint8 = np.array([[[1, 2, 3]]], dtype=np.uint8)
    img_uint8 = fim.Image.from_numpy(arr_uint8)
    repr_str = repr(img_uint8)
    assert "dtype=uint8" in repr_str
    assert "width=1" in repr_str
    assert "height=1" in repr_str
    assert "channels=3" in repr_str

    # Test uint16
    arr_uint16 = np.array([[[100, 200]]], dtype=np.uint16)
    img_uint16 = fim.Image.from_numpy(arr_uint16)
    repr_str = repr(img_uint16)
    assert "dtype=uint16" in repr_str

    # Test float32
    arr_float = np.array([[[0.1]]], dtype=np.float32)
    img_float = fim.Image.from_numpy(arr_float)
    repr_str = repr(img_float)
    assert "dtype=float32" in repr_str


def test_png_uint16_support():
    """Test that PNG sinks support uint16."""
    arr_uint16 = np.arange(4 * 3 * 3, dtype=np.uint16).reshape(4, 3, 3) * 1000
    img_uint16 = fim.Image.from_numpy(arr_uint16)

    # Test write_png (lodepng)
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as f:
        fname_lodepng = f.name
    try:
        img_uint16.write_png(fname_lodepng)
        # Verify file was created
        assert os.path.exists(fname_lodepng)
        assert os.path.getsize(fname_lodepng) > 0
    finally:
        if os.path.exists(fname_lodepng):
            os.unlink(fname_lodepng)

    # Test write_spng
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as f:
        fname_spng = f.name
    try:
        img_uint16.write_spng(fname_spng)
        # Verify file was created
        assert os.path.exists(fname_spng)
        assert os.path.getsize(fname_spng) > 0
    finally:
        if os.path.exists(fname_spng):
            os.unlink(fname_spng)


def test_png_float32_error():
    """Test that PNG sinks give clear error for float32."""
    arr_float = np.array([[[0.1, 0.2, 0.3]]], dtype=np.float32)
    img_float = fim.Image.from_numpy(arr_float)

    # Should raise an error with helpful message
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as f:
        fname = f.name

    try:
        with pytest.raises(RuntimeError) as exc_info:
            img_float.write_png(fname)
        error_msg = str(exc_info.value)
        assert "only supports uint8 and uint16" in error_msg.lower()
        assert "float32" in error_msg.lower()
    finally:
        if os.path.exists(fname):
            os.unlink(fname)

    # Same test for write_spng
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as f:
        fname = f.name
    try:
        with pytest.raises(RuntimeError) as exc_info:
            img_float.write_spng(fname)
        error_msg = str(exc_info.value)
        assert "only supports uint8 and uint16" in error_msg.lower()
        assert "float32" in error_msg.lower()
    finally:
        if os.path.exists(fname):
            os.unlink(fname)


def test_compression_round_trip_all_types():
    """Test compression/decompression works for all pixel types."""
    # Test uint8 with LZ4
    arr_uint8 = np.arange(10 * 8 * 3, dtype=np.uint8).reshape(10, 8, 3)
    img_uint8 = fim.Image.from_numpy(arr_uint8)

    with tempfile.NamedTemporaryFile(suffix=".fimage", delete=False) as f:
        fname = f.name
    try:
        img_uint8.write_fimage(fname, compression=fim.CompressionType.LZ4)
        loaded = fim.Image.from_fimage(fname)
        result = loaded.to_numpy()
        assert result.dtype == np.uint8
        np.testing.assert_array_equal(result, arr_uint8)
    finally:
        if os.path.exists(fname):
            os.unlink(fname)

    # Test uint16 with ZSTD
    arr_uint16 = np.arange(6 * 4 * 2, dtype=np.uint16).reshape(6, 4, 2) * 100
    img_uint16 = fim.Image.from_numpy(arr_uint16)

    with tempfile.NamedTemporaryFile(suffix=".fimage", delete=False) as f:
        fname = f.name
    try:
        img_uint16.write_fimage(fname, compression=fim.CompressionType.ZSTD)
        loaded = fim.Image.from_fimage(fname)
        result = loaded.to_numpy()
        assert result.dtype == np.uint16
        np.testing.assert_array_equal(result, arr_uint16)
    finally:
        if os.path.exists(fname):
            os.unlink(fname)

    # Test float32 with LZ4
    arr_float = np.arange(5 * 4 * 1, dtype=np.float32).reshape(5, 4, 1) * 0.5
    img_float = fim.Image.from_numpy(arr_float)

    with tempfile.NamedTemporaryFile(suffix=".fimage", delete=False) as f:
        fname = f.name
    try:
        img_float.write_fimage(fname, compression=fim.CompressionType.LZ4)
        loaded = fim.Image.from_fimage(fname)
        result = loaded.to_numpy()
        assert result.dtype == np.float32
        np.testing.assert_array_almost_equal(result, arr_float)
    finally:
        if os.path.exists(fname):
            os.unlink(fname)


if __name__ == "__main__":
    # Run tests manually
    test_uint8_round_trip()
    print("✓ uint8 round-trip test passed")

    test_uint16_round_trip()
    print("✓ uint16 round-trip test passed")

    test_float32_round_trip()
    print("✓ float32 round-trip test passed")

    test_uint16_fimage_write_read()
    print("✓ uint16 FImage write/read test passed")

    test_float32_fimage_write_read()
    print("✓ float32 FImage write/read test passed")

    test_get_tile_with_different_dtypes()
    print("✓ get_tile dtype test passed")

    test_pixel_type_enum_exposed()
    print("✓ PixelType enum test passed")

    test_image_dtype_property()
    print("✓ Image dtype property test passed")

    test_image_repr_includes_dtype()
    print("✓ Image __repr__ dtype test passed")

    test_compression_round_trip_all_types()
    print("✓ Compression round-trip test passed")

    test_png_uint16_support()
    print("✓ PNG uint16 support test passed")

    test_png_float32_error()
    print("✓ PNG float32 error message test passed")

    print("\n✅ All multi-type tests passed!")

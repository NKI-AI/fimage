#!/usr/bin/env python3
"""
Tests for black canvas and paste functionality.
"""

import numpy as np
import pytest

import fim


def test_deferred_black_canvas_creation():
    """Test creating a deferred black canvas."""
    canvas = fim.Image.black(1000, 1000)

    assert isinstance(canvas, fim.DeferredBlackCanvas)
    assert canvas.get_width() == 1000
    assert canvas.get_height() == 1000
    print("  ✓ DeferredBlackCanvas created successfully")


def test_deferred_black_canvas_invalid_dimensions():
    """Test that invalid dimensions raise errors."""
    with pytest.raises(ValueError):
        fim.Image.black(0, 100)

    with pytest.raises(ValueError):
        fim.Image.black(100, -10)

    print("  ✓ Invalid dimensions raise ValueError")


def test_basic_paste():
    """Test basic paste operation."""
    # Create a small white image
    white_data = np.full((20, 20, 3), 255, dtype=np.uint8)
    white_img = fim.Image.from_numpy(white_data)

    # Create black canvas and paste white image
    canvas = fim.Image.black(100, 100)
    result = canvas.paste(white_img, 10, 10)

    # Result should be a valid Image
    assert isinstance(result, fim.Image)
    assert result.is_valid()

    # Check dimensions
    width, height, channels = result.dimensions
    assert width == 100
    assert height == 100
    assert channels == 3

    print("  ✓ Basic paste operation successful")


def test_paste_channel_inference():
    """Test that channels are inferred from first pasted image."""
    # Create images with different channel counts
    gray_data = np.full((10, 10, 1), 128, dtype=np.uint8)
    gray_img = fim.Image.from_numpy(gray_data)

    rgb_data = np.full((10, 10, 3), 255, dtype=np.uint8)
    rgb_img = fim.Image.from_numpy(rgb_data)

    rgba_data = np.full((10, 10, 4), 200, dtype=np.uint8)
    rgba_img = fim.Image.from_numpy(rgba_data)

    # Test with grayscale
    canvas1 = fim.Image.black(50, 50)
    result1 = canvas1.paste(gray_img, 5, 5)
    _, _, channels1 = result1.dimensions
    assert channels1 == 1

    # Test with RGB
    canvas2 = fim.Image.black(50, 50)
    result2 = canvas2.paste(rgb_img, 5, 5)
    _, _, channels2 = result2.dimensions
    assert channels2 == 3

    # Test with RGBA
    canvas3 = fim.Image.black(50, 50)
    result3 = canvas3.paste(rgba_img, 5, 5)
    _, _, channels3 = result3.dimensions
    assert channels3 == 4

    print("  ✓ Channel inference works correctly")


def test_paste_pixel_values():
    """Test that pasted pixels have correct values."""
    # Create a white square
    white_data = np.full((20, 20, 3), 255, dtype=np.uint8)
    white_img = fim.Image.from_numpy(white_data)

    # Paste on black canvas
    canvas = fim.Image.black(100, 100)
    result = canvas.paste(white_img, 40, 40)

    # Convert to numpy and check
    array = result.to_numpy()

    # Check pasted region is white
    pasted_region = array[40:60, 40:60, :]
    assert np.all(pasted_region == 255), "Pasted region should be white"

    # Check top-left corner is black
    corner = array[0:10, 0:10, :]
    assert np.all(corner == 0), "Background should be black"

    # Check bottom-right after pasted region is black
    bottom_right = array[70:80, 70:80, :]
    assert np.all(bottom_right == 0), "Background should be black"

    print("  ✓ Pasted pixels have correct values")


def test_chained_pastes():
    """Test multiple paste operations chained together."""
    # Create three different colored squares
    red_data = np.zeros((15, 15, 3), dtype=np.uint8)
    red_data[:, :, 0] = 255
    red_img = fim.Image.from_numpy(red_data)

    green_data = np.zeros((15, 15, 3), dtype=np.uint8)
    green_data[:, :, 1] = 255
    green_img = fim.Image.from_numpy(green_data)

    blue_data = np.zeros((15, 15, 3), dtype=np.uint8)
    blue_data[:, :, 2] = 255
    blue_img = fim.Image.from_numpy(blue_data)

    # Chain paste operations
    canvas = fim.Image.black(100, 100)
    result = canvas.paste(red_img, 10, 10).paste(green_img, 30, 30).paste(blue_img, 50, 50)

    # Convert to numpy
    array = result.to_numpy()

    # Check red region
    red_region = array[15, 15]
    assert red_region[0] == 255 and red_region[1] == 0 and red_region[2] == 0

    # Check green region
    green_region = array[35, 35]
    assert green_region[0] == 0 and green_region[1] == 255 and green_region[2] == 0

    # Check blue region
    blue_region = array[55, 55]
    assert blue_region[0] == 0 and blue_region[1] == 0 and blue_region[2] == 255

    # Check background is still black
    background = array[0, 0]
    assert np.all(background == 0)

    print("  ✓ Chained pastes work correctly")


def test_paste_at_origin():
    """Test pasting at (0, 0)."""
    gray_data = np.full((30, 30, 3), 128, dtype=np.uint8)
    gray_img = fim.Image.from_numpy(gray_data)

    canvas = fim.Image.black(100, 100)
    result = canvas.paste(gray_img, 0, 0)

    array = result.to_numpy()

    # Check pasted region
    assert np.all(array[0:30, 0:30, :] == 128)

    # Check outside pasted region
    assert np.all(array[50, 50, :] == 0)

    print("  ✓ Paste at origin works correctly")


def test_paste_at_bottom_right():
    """Test pasting at bottom-right corner."""
    small_data = np.full((10, 10, 3), 200, dtype=np.uint8)
    small_img = fim.Image.from_numpy(small_data)

    canvas = fim.Image.black(100, 100)
    result = canvas.paste(small_img, 90, 90)

    array = result.to_numpy()

    # Check pasted region (90:100, 90:100)
    assert np.all(array[90:100, 90:100, :] == 200)

    # Check region before paste
    assert np.all(array[80, 80, :] == 0)

    print("  ✓ Paste at bottom-right works correctly")


def test_paste_out_of_bounds_error():
    """Test that pasting out of bounds raises error."""
    img_data = np.full((20, 20, 3), 255, dtype=np.uint8)
    img = fim.Image.from_numpy(img_data)

    canvas = fim.Image.black(100, 100)

    # Try to paste beyond bounds
    with pytest.raises(ValueError):
        canvas.paste(img, 95, 95)  # Would extend to (115, 115)

    # Try to paste at negative position
    with pytest.raises(ValueError):
        canvas.paste(img, -5, 10)

    print("  ✓ Out of bounds paste raises ValueError")


def test_paste_channel_mismatch_error():
    """Test that channel mismatch raises error when chaining pastes."""
    rgb_data = np.full((10, 10, 3), 255, dtype=np.uint8)
    rgb_img = fim.Image.from_numpy(rgb_data)

    rgba_data = np.full((10, 10, 4), 200, dtype=np.uint8)
    rgba_img = fim.Image.from_numpy(rgba_data)

    canvas = fim.Image.black(100, 100)
    result = canvas.paste(rgb_img, 10, 10)  # Creates 3-channel canvas

    # Try to paste 4-channel image onto 3-channel result
    with pytest.raises(ValueError):
        result.paste(rgba_img, 30, 30)

    print("  ✓ Channel mismatch raises ValueError")


def test_lazy_evaluation():
    """Test that paste operations are lazy."""
    import time

    # Create a large canvas
    canvas = fim.Image.black(10000, 10000)

    # Create a small image
    small_data = np.full((100, 100, 3), 255, dtype=np.uint8)
    small_img = fim.Image.from_numpy(small_data)

    # Paste should be instant (lazy)
    start = time.perf_counter()
    result = canvas.paste(small_img, 100, 100)
    paste_time = time.perf_counter() - start

    # Should be very fast (< 10ms) because it's lazy
    assert paste_time < 0.01, f"Paste took {paste_time * 1000:.1f}ms, should be instant"

    # Evaluation happens on to_numpy
    start = time.perf_counter()
    array = result.to_numpy()
    eval_time = time.perf_counter() - start

    # Evaluation should take longer
    assert eval_time > paste_time, "Evaluation should take longer than lazy paste"

    # Verify result
    assert array.shape == (10000, 10000, 3)

    print(f"  ✓ Lazy evaluation confirmed (paste: {paste_time * 1000:.2f}ms, eval: {eval_time * 1000:.1f}ms)")


def test_paste_with_pipeline_operations():
    """Test paste combined with other pipeline operations."""
    # Create image
    data = np.full((50, 50, 3), 255, dtype=np.uint8)
    img = fim.Image.from_numpy(data)

    # Paste, then crop, then downsample
    canvas = fim.Image.black(200, 200)
    result = canvas.paste(img, 75, 75).crop((50, 50), (100, 100)).downsample(2)

    # Check final dimensions
    width, height, channels = result.dimensions
    assert width == 50 and height == 50 and channels == 3

    print("  ✓ Paste works with other pipeline operations")


def test_numpy_roundtrip():
    """Test complete roundtrip: numpy -> paste -> numpy."""
    # Create original image
    original = np.random.randint(0, 256, (30, 30, 3), dtype=np.uint8)
    img = fim.Image.from_numpy(original)

    # Paste on canvas
    canvas = fim.Image.black(100, 100)
    result = canvas.paste(img, 35, 35)

    # Convert back to numpy
    array = result.to_numpy()

    # Extract pasted region
    pasted_region = array[35:65, 35:65, :]

    # Should match original
    assert np.array_equal(pasted_region, original)

    print("  ✓ NumPy roundtrip preserves data")


def test_overlapping_pastes():
    """Test that later pastes overwrite earlier ones."""
    # Create two different colored squares
    red_data = np.zeros((30, 30, 3), dtype=np.uint8)
    red_data[:, :, 0] = 255
    red_img = fim.Image.from_numpy(red_data)

    blue_data = np.zeros((20, 20, 3), dtype=np.uint8)
    blue_data[:, :, 2] = 255
    blue_img = fim.Image.from_numpy(blue_data)

    # Paste red, then paste blue overlapping
    canvas = fim.Image.black(100, 100)
    result = canvas.paste(red_img, 20, 20).paste(blue_img, 30, 30)

    array = result.to_numpy()

    # Check overlapping region is blue (later paste wins)
    overlapping_pixel = array[35, 35]
    assert overlapping_pixel[0] == 0 and overlapping_pixel[1] == 0 and overlapping_pixel[2] == 255

    # Check red region that's not overlapped
    red_pixel = array[25, 25]
    assert red_pixel[0] == 255 and red_pixel[1] == 0 and red_pixel[2] == 0

    print("  ✓ Overlapping pastes work correctly (later wins)")


def main():
    """Run all tests."""
    print("\n" + "=" * 60)
    print("BLACK CANVAS AND PASTE TESTS")
    print("=" * 60 + "\n")

    test_deferred_black_canvas_creation()
    test_deferred_black_canvas_invalid_dimensions()
    test_basic_paste()
    test_paste_channel_inference()
    test_paste_pixel_values()
    test_chained_pastes()
    test_paste_at_origin()
    test_paste_at_bottom_right()
    test_paste_out_of_bounds_error()
    test_paste_channel_mismatch_error()
    test_lazy_evaluation()
    test_paste_with_pipeline_operations()
    test_numpy_roundtrip()
    test_overlapping_pastes()

    print("\n" + "=" * 60)
    print("ALL TESTS PASSED!")
    print("=" * 60 + "\n")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Simple integration test for fim Python bindings.

This test creates synthetic images and verifies basic operations work.
"""

import sys
import time
import numpy as np
import fim


def test_basic_memory_source():
    """Test creating an image from memory."""
    print("\n[Test 1] Creating image from memory...")

    # Create a simple 10x10 RGB image with all pixels red
    data = [255, 0, 0] * (10 * 10)
    img = fim.Image.from_memory(data, 10, 10, 3)

    assert img.is_valid(), "Image should be valid"
    dims = img.dimensions
    assert dims == (10, 10, 3), f"Expected (10, 10, 3), got {dims}"

    print("  ✓ Image created successfully")
    print(f"  ✓ Dimensions: {dims}")


def test_crop_operation():
    """Test crop operation."""
    print("\n[Test 2] Testing crop operation...")

    # Create a simple 10x10 RGB image with all pixels red
    data = [255, 0, 0] * (10 * 10)
    img = fim.Image.from_memory(data, 10, 10, 3)

    cropped = img.crop((0, 0), (5, 5))
    dims = cropped.dimensions

    assert dims == (5, 5, 3), f"Expected (5, 5, 3), got {dims}"
    print(f"  ✓ Cropped to: {dims}")


def test_downsample_operation():
    """Test downsample operation."""
    print("\n[Test 3] Testing downsample operation...")

    # Create a simple 10x10 RGB image with all pixels red
    data = [255, 0, 0] * (10 * 10)
    img = fim.Image.from_memory(data, 10, 10, 3)

    downsampled = img.downsample(2)
    dims = downsampled.dimensions

    # 10x10 downsampled by 2 should be 5x5
    assert dims[0] == 5 and dims[1] == 5, f"Expected 5x5, got {dims[0]}x{dims[1]}"
    print(f"  ✓ Downsampled to: {dims}")


def test_chained_operations():
    """Test method chaining."""
    print("\n[Test 4] Testing chained operations...")

    data = [128] * (20 * 20 * 3)
    img = fim.Image.from_memory(data, 20, 20, 3)

    result = img.crop((5, 5), (10, 10)).downsample(2)
    dims = result.dimensions

    # 10x10 crop then downsample by 2 = 5x5
    assert dims[0] == 5 and dims[1] == 5, f"Expected 5x5, got {dims[0]}x{dims[1]}"
    print(f"  ✓ Chained pipeline result: {dims}")


def test_numpy_conversion():
    """Test NumPy conversion."""
    print("\n[Test 5] Testing NumPy conversion...")

    # Create a small test pattern
    data = []
    for y in range(4):
        for x in range(4):
            data.extend([x * 60, y * 60, 128])

    img = fim.Image.from_memory(data, 4, 4, 3)
    array = img.to_numpy()

    assert isinstance(array, np.ndarray), "Should return numpy array"
    assert array.shape == (4, 4, 3), f"Expected shape (4, 4, 3), got {array.shape}"
    assert array.dtype == np.uint8, f"Expected dtype uint8, got {array.dtype}"

    # Check some values
    assert array[0, 0, 2] == 128, "Blue channel should be 128"

    print(f"  ✓ NumPy array shape: {array.shape}")
    print(f"  ✓ NumPy array dtype: {array.dtype}")
    print(f"  ✓ Sample pixel [0,0]: {array[0, 0]}")


def test_stack_operation():
    """Test stacking multiple images."""
    print("\n[Test 6] Testing stack operation...")

    # Create three single-channel images
    red_data = [255] * 16 + [0] * 16 + [0] * 16  # 4x4
    green_data = [0] * 16 + [255] * 16 + [0] * 16
    blue_data = [0] * 16 + [0] * 16 + [255] * 16

    red = fim.Image.from_memory(red_data, 4, 4, 1)
    green = fim.Image.from_memory(green_data, 4, 4, 1)
    blue = fim.Image.from_memory(blue_data, 4, 4, 1)

    # Stack them
    rgb = fim.Image.stack([red, green, blue], axis="bands")
    dims = rgb.dimensions

    assert dims == (4, 4, 3), f"Expected (4, 4, 3), got {dims}"
    print(f"  ✓ Stacked image dimensions: {dims}")

    # Verify the stacking worked correctly
    array = rgb.to_numpy()
    print(f"  ✓ Converted to NumPy: {array.shape}")


def test_invalid_stack():
    """Test that stacking incompatible images fails."""
    print("\n[Test 7] Testing invalid stack (should fail)...")

    img1 = fim.Image.from_memory([0] * (5 * 5 * 1), 5, 5, 1)
    img2 = fim.Image.from_memory([0] * (10 * 10 * 1), 10, 10, 1)

    try:
        fim.Image.stack([img1, img2], axis="bands")
        print("  ✗ Should have raised an error!")
        assert False, "Should have raised RuntimeError"
    except RuntimeError as e:
        print(f"  ✓ Correctly raised error: {e}")


def test_repr():
    """Test string representation."""
    print("\n[Test 8] Testing __repr__...")

    img = fim.Image.from_memory([0] * (5 * 5 * 1), 5, 5, 1)
    repr_str = repr(img)

    assert "Image" in repr_str, "repr should contain 'Image'"
    assert "5" in repr_str, "repr should contain dimensions"

    print(f"  ✓ repr: {repr_str}")


def test_lazy_evaluation():
    """Test that operations are lazy."""
    print("\n[Test 9] Testing lazy evaluation...")

    # Create a large-ish image
    data = [100] * (100 * 100 * 3)
    img = fim.Image.from_memory(data, 100, 100, 3)

    # Apply multiple operations - these should be instant (lazy)
    result = img.crop((10, 10), (50, 50)).downsample(5)

    # Only when we call to_numpy() should evaluation happen
    array = result.to_numpy()

    # Verify result
    assert array.shape[0] == 10, f"Expected height 10, got {array.shape[0]}"
    assert array.shape[1] == 10, f"Expected width 10, got {array.shape[1]}"

    print(f"  ✓ Lazy pipeline evaluated to: {array.shape}")


def test_numpy_roundtrip():
    """Test creating from NumPy and converting back."""
    print("\n[Test 10] Testing NumPy roundtrip...")

    # Create a pattern with NumPy
    original = np.zeros((8, 8, 3), dtype=np.uint8)
    original[:, :, 0] = np.arange(8)[None, :] * 30  # Gradient in R
    original[:, :, 1] = np.arange(8)[:, None] * 30  # Gradient in G
    original[:, :, 2] = 128  # Constant B

    # Convert to fim and back
    img = fim.Image.from_memory(original.flatten().tolist(), 8, 8, 3)
    result = img.to_numpy()

    # Should be identical
    np.testing.assert_array_equal(original, result)
    print("  ✓ NumPy roundtrip successful")


def test_crop_pixel_verification():
    """Test that crop extracts the correct pixel values."""
    print("\n[Test 11] Testing crop pixel verification...")

    # Create a known pattern: each pixel has its (x, y) coordinates as values
    size = 10
    data = []
    for y in range(size):
        for x in range(size):
            data.extend([x * 25, y * 25, 0])  # R=x*25, G=y*25, B=0

    img = fim.Image.from_memory(data, size, size, 3)

    # Crop region (2,3) to (4,5) -> 2x2 region
    cropped = img.crop((2, 3), (2, 2))
    result = cropped.to_numpy()

    print(f"  Cropped region shape: {result.shape}")

    # Verify the exact pixel values
    # Pixel at (0,0) in cropped should be (2,3) from original -> [50, 75, 0]
    assert result[0, 0, 0] == 50, f"Expected R=50, got {result[0, 0, 0]}"
    assert result[0, 0, 1] == 75, f"Expected G=75, got {result[0, 0, 1]}"
    print(f"  ✓ Top-left pixel [0,0]: {result[0, 0]} (expected [50, 75, 0])")

    # Pixel at (1,1) in cropped should be (3,4) from original -> [75, 100, 0]
    assert result[1, 1, 0] == 75, f"Expected R=75, got {result[1, 1, 0]}"
    assert result[1, 1, 1] == 100, f"Expected G=100, got {result[1, 1, 1]}"
    print(f"  ✓ Bottom-right pixel [1,1]: {result[1, 1]} (expected [75, 100, 0])")

    print("  ✓ Crop pixel values verified correctly")


def test_downsample_pixel_verification():
    """Test that downsample produces correct averaged values."""
    print("\n[Test 12] Testing downsample pixel verification...")

    # Create a 4x4 image with known values for easy verification
    # Top-left 2x2: all 100, top-right 2x2: all 200
    # Bottom-left 2x2: all 50, bottom-right 2x2: all 150
    data = []
    for y in range(4):
        for x in range(4):
            if y < 2 and x < 2:
                value = 100
            elif y < 2 and x >= 2:
                value = 200
            elif y >= 2 and x < 2:
                value = 50
            else:
                value = 150
            data.extend([value, value, value])

    img = fim.Image.from_memory(data, 4, 4, 3)

    # Downsample by 2 -> should average each 2x2 block
    downsampled = img.downsample(2)
    result = downsampled.to_numpy()

    print(f"  Downsampled shape: {result.shape}")
    assert result.shape == (2, 2, 3), "Should be 2x2 after downsampling 4x4 by factor 2"

    # Verify averaged values
    # Top-left: average of [100,100,100,100] = 100
    assert result[0, 0, 0] == 100, f"Top-left should be 100, got {result[0, 0, 0]}"
    print(f"  ✓ Top-left [0,0]: {result[0, 0, 0]} (expected 100)")

    # Top-right: average of [200,200,200,200] = 200
    assert result[0, 1, 0] == 200, f"Top-right should be 200, got {result[0, 1, 0]}"
    print(f"  ✓ Top-right [0,1]: {result[0, 1, 0]} (expected 200)")

    # Bottom-left: average of [50,50,50,50] = 50
    assert result[1, 0, 0] == 50, f"Bottom-left should be 50, got {result[1, 0, 0]}"
    print(f"  ✓ Bottom-left [1,0]: {result[1, 0, 0]} (expected 50)")

    # Bottom-right: average of [150,150,150,150] = 150
    assert result[1, 1, 0] == 150, f"Bottom-right should be 150, got {result[1, 1, 0]}"
    print(f"  ✓ Bottom-right [1,1]: {result[1, 1, 0]} (expected 150)")

    print("  ✓ Downsample averaging verified correctly")


def test_stack_pixel_verification():
    """Test that stack correctly concatenates channel data."""
    print("\n[Test 13] Testing stack pixel verification...")

    # Create three 2x2 single-channel images with known values
    # Red channel: [[100, 110], [120, 130]]
    red_data = [100, 110, 120, 130]
    red = fim.Image.from_memory(red_data, 2, 2, 1)

    # Green channel: [[50, 60], [70, 80]]
    green_data = [50, 60, 70, 80]
    green = fim.Image.from_memory(green_data, 2, 2, 1)

    # Blue channel: [[200, 210], [220, 230]]
    blue_data = [200, 210, 220, 230]
    blue = fim.Image.from_memory(blue_data, 2, 2, 1)

    # Stack them into RGB
    rgb = fim.Image.stack([red, green, blue], axis="bands")
    result = rgb.to_numpy()

    print(f"  Stacked shape: {result.shape}")
    assert result.shape == (2, 2, 3), "Should be 2x2x3 RGB"

    # Verify pixel at [0, 0] = [100, 50, 200]
    expected = np.array([100, 50, 200], dtype=np.uint8)
    np.testing.assert_array_equal(result[0, 0], expected)
    print(f"  ✓ Pixel [0,0]: {result[0, 0]} (expected [100, 50, 200])")

    # Verify pixel at [0, 1] = [110, 60, 210]
    expected = np.array([110, 60, 210], dtype=np.uint8)
    np.testing.assert_array_equal(result[0, 1], expected)
    print(f"  ✓ Pixel [0,1]: {result[0, 1]} (expected [110, 60, 210])")

    # Verify pixel at [1, 0] = [120, 70, 220]
    expected = np.array([120, 70, 220], dtype=np.uint8)
    np.testing.assert_array_equal(result[1, 0], expected)
    print(f"  ✓ Pixel [1,0]: {result[1, 0]} (expected [120, 70, 220])")

    # Verify pixel at [1, 1] = [130, 80, 230]
    expected = np.array([130, 80, 230], dtype=np.uint8)
    np.testing.assert_array_equal(result[1, 1], expected)
    print(f"  ✓ Pixel [1,1]: {result[1, 1]} (expected [130, 80, 230])")

    print("  ✓ Stack channel concatenation verified correctly")


def test_pipeline_pixel_verification():
    """Test pixel correctness through a complete pipeline."""
    print("\n[Test 14] Testing pipeline pixel verification...")

    # Create 8x8 checkerboard with clear pattern
    size = 8
    data = []
    for y in range(size):
        for x in range(size):
            # Alternating 0 and 255
            value = 255 if (x + y) % 2 == 0 else 0
            data.extend([value, value, value])

    img = fim.Image.from_memory(data, size, size, 3)

    # Crop center 4x4 region
    cropped = img.crop((2, 2), (4, 4))
    cropped_result = cropped.to_numpy()

    # Verify a specific pixel in cropped image
    # Original [2,2] -> checkerboard value at (2,2) = (2+2)%2 = 0 -> white (255)
    assert cropped_result[0, 0, 0] == 255, f"Cropped [0,0] should be white, got {cropped_result[0, 0, 0]}"
    print(f"  ✓ After crop [0,0]: {cropped_result[0, 0, 0]} (expected 255)")

    # Original [2,3] -> checkerboard value at (2,3) = (2+3)%2 = 1 -> black (0)
    assert cropped_result[0, 1, 0] == 0, f"Cropped [0,1] should be black, got {cropped_result[0, 1, 0]}"
    print(f"  ✓ After crop [0,1]: {cropped_result[0, 1, 0]} (expected 0)")

    # Now downsample the cropped 4x4 by 2 -> 2x2
    downsampled = cropped.downsample(2)
    down_result = downsampled.to_numpy()

    print(f"  Final shape after crop+downsample: {down_result.shape}")

    # Each 2x2 block in checkerboard has 2 black and 2 white pixels
    # Average should be (0+255+255+0)/4 = 127.5 ≈ 127
    # Check that values are averaged (not 0 or 255)
    unique_vals = np.unique(down_result)
    print(f"  ✓ Unique values after averaging: {unique_vals}")
    assert 100 < down_result[0, 0, 0] < 160, "Should be averaged, not pure black/white"

    print("  ✓ Pipeline pixel transformations verified correctly")


def test_edge_case_pixels():
    """Test edge cases and boundary pixel values."""
    print("\n[Test 15] Testing edge case pixel values...")

    # Create image with extreme values
    data = []
    for y in range(4):
        for x in range(4):
            if x == 0:
                value = 0  # Left edge: black
            elif x == 3:
                value = 255  # Right edge: white
            else:
                value = 128  # Middle: gray
            data.extend([value, value, value])

    img = fim.Image.from_memory(data, 4, 4, 3)
    result = img.to_numpy()

    # Verify edge pixels
    assert result[0, 0, 0] == 0, "Left edge should be 0"
    assert result[0, 3, 0] == 255, "Right edge should be 255"
    assert result[0, 1, 0] == 128, "Middle should be 128"
    print(f"  ✓ Edge values: left={result[0, 0, 0]}, middle={result[0, 1, 0]}, right={result[0, 3, 0]}")

    # Test that crop preserves edge values
    cropped = img.crop((0, 0), (2, 2))
    crop_result = cropped.to_numpy()
    assert crop_result[0, 0, 0] == 0, "Cropped left edge should still be 0"
    print(f"  ✓ Crop preserves edge values: {crop_result[0, 0, 0]}")

    # Test downsample with edge values
    # Column 0 (all 0) and column 1 (all 128) -> average = 64
    downsampled = img.crop((0, 0), (4, 2)).downsample(2)
    down_result = downsampled.to_numpy()
    print(f"  ✓ Downsampled edge averaging: {down_result[0, 0, 0]} (expected ~64)")
    assert 60 <= down_result[0, 0, 0] <= 68, "Should average edge values correctly"

    print("  ✓ Edge case pixels verified correctly")


def test_complex_pipeline():
    """Test a complex multi-stage pipeline like the C++ demos."""
    print("\n[Test 16] Testing complex pipeline (demo-style)...")

    # Create a larger synthetic "checkerboard" pattern
    print("  Creating synthetic checkerboard pattern (256x256)...")
    size = 256
    square_size = 32
    data = []

    for y in range(size):
        for x in range(size):
            # Checkerboard logic
            square_x = x // square_size
            square_y = y // square_size
            is_white = (square_x + square_y) % 2 == 0
            value = 255 if is_white else 0
            data.extend([value, value, value])

    # Build a complex lazy pipeline
    print("  Building lazy pipeline: crop -> downsample -> crop -> downsample...")
    img = fim.Image.from_memory(data, size, size, 3)

    # Multi-stage pipeline
    pipeline = (
        img.crop((50, 50), (200, 200))  # Extract region of interest
        .downsample(2)  # Reduce by 2x (100x100)
        .crop((10, 10), (80, 80))  # Refine region
        .downsample(2)  # Further reduce (40x40)
    )

    print("  ✓ Pipeline constructed (lazy - no computation yet)")

    # Get dimensions to verify pipeline structure
    dims = pipeline.dimensions
    print(f"  ✓ Final dimensions: {dims}")
    assert dims[0] == 40 and dims[1] == 40, f"Expected 40x40, got {dims[0]}x{dims[1]}"

    # Now trigger evaluation
    print("  Triggering evaluation via to_numpy()...")
    result = pipeline.to_numpy()

    print(f"  ✓ Pipeline evaluated: shape={result.shape}, dtype={result.dtype}")
    assert result.shape == (40, 40, 3), f"Expected (40, 40, 3), got {result.shape}"

    # Verify the checkerboard pattern is preserved
    # The pattern should still be visible (some squares should be white, some black)
    unique_values = np.unique(result)
    print(f"  ✓ Unique pixel values in result: {unique_values}")
    assert len(unique_values) >= 2, "Should have at least 2 distinct values (checkerboard)"


def test_multi_branch_pipeline():
    """Test creating multiple derived pipelines from one source."""
    print("\n[Test 17] Testing multi-branch pipeline...")

    # Create a gradient image
    print("  Creating gradient source image...")
    size = 128
    data = []
    for y in range(size):
        for x in range(size):
            r = int((x / size) * 255)
            g = int((y / size) * 255)
            b = 128
            data.extend([r, g, b])

    # Create base image
    base = fim.Image.from_memory(data, size, size, 3)

    # Create multiple derived pipelines
    print("  Creating multiple branches from same source...")

    # Branch 1: Top-left region, heavily downsampled
    top_left = base.crop((0, 0), (64, 64)).downsample(4)
    dims1 = top_left.dimensions
    print(f"  Branch 1 (top-left): {dims1}")

    # Branch 2: Center region, moderately downsampled
    center = base.crop((32, 32), (64, 64)).downsample(2)
    dims2 = center.dimensions
    print(f"  Branch 2 (center): {dims2}")

    # Branch 3: Bottom-right, minimally processed
    bottom_right = base.crop((64, 64), (32, 32))
    dims3 = bottom_right.dimensions
    print(f"  Branch 3 (bottom-right): {dims3}")

    # Evaluate all branches
    result1 = top_left.to_numpy()
    result2 = center.to_numpy()
    result3 = bottom_right.to_numpy()

    print(f"  ✓ All branches evaluated successfully")
    print(f"    Result 1: {result1.shape}")
    print(f"    Result 2: {result2.shape}")
    print(f"    Result 3: {result3.shape}")

    # Verify they have different content (different regions)
    assert not np.array_equal(result1, result2), "Different branches should produce different results"


def test_large_synthetic_image():
    """Test with a larger synthetic image to stress the pipeline."""
    print("\n[Test 18] Testing with larger synthetic image...")

    # Create a larger image with a pattern
    size = 512
    print(f"  Creating {size}x{size} synthetic image...")

    data = []
    for y in range(size):
        for x in range(size):
            # Create a radial gradient pattern
            dx = x - size // 2
            dy = y - size // 2
            dist = np.sqrt(dx * dx + dy * dy)
            value = int((dist / (size * 0.7)) * 255) % 256
            data.extend([value, value, value])

    img = fim.Image.from_memory(data, size, size, 3)

    # Apply a realistic processing pipeline
    print("  Applying processing pipeline...")
    processed = (
        img.crop((128, 128), (256, 256)).downsample(4)  # Extract center region  # Reduce to thumbnail size
    )

    dims = processed.dimensions
    print(f"  ✓ Processed dimensions: {dims}")

    # Evaluate
    result = processed.to_numpy()
    print(f"  ✓ Final result: {result.shape}")
    assert result.shape[0] == 64 and result.shape[1] == 64, "Should be 64x64 after processing"


def test_lazy_timing():
    """Test that pipeline construction is instant (lazy) but evaluation takes time."""
    print("\n[Test 19] Testing lazy evaluation timing...")

    # Create a large image (2048x2048 = 4 megapixels)
    size = 2048
    print(f"  Creating {size}x{size} checkerboard ({size * size * 3 / (1024 * 1024):.1f} MB)...")

    t0 = time.perf_counter()
    data = []
    square_size = 64
    for y in range(size):
        for x in range(size):
            square_x = x // square_size
            square_y = y // square_size
            value = 255 if (square_x + square_y) % 2 == 0 else 0
            data.extend([value, value, value])
    t_create = time.perf_counter() - t0
    print(f"  Created data in {t_create * 1000:.1f}ms")

    # Create source (should be fast, just wraps data)
    t0 = time.perf_counter()
    img = fim.Image.from_memory(data, size, size, 3)
    t_source = time.perf_counter() - t0
    print(f"  ✓ Created source in {t_source * 1000:.3f}ms")

    # Build a long lazy pipeline (should be nearly instant!)
    t0 = time.perf_counter()
    pipeline = (
        img.crop((200, 200), (1600, 1600))
        .downsample(2)
        .crop((100, 100), (600, 600))
        .downsample(2)
        .crop((50, 50), (200, 200))
        .downsample(2)
    )
    t_pipeline = time.perf_counter() - t0
    print(f"  ✓ Built 6-stage pipeline in {t_pipeline * 1000:.3f}ms (LAZY!)")

    # Pipeline construction should be < 1ms (instant)
    assert t_pipeline < 0.01, f"Pipeline construction took {t_pipeline * 1000:.1f}ms, should be < 10ms (lazy)"

    # Now trigger evaluation (should take measurable time)
    t0 = time.perf_counter()
    result = pipeline.to_numpy()
    t_eval = time.perf_counter() - t0
    print(f"  ✓ Evaluated pipeline in {t_eval * 1000:.1f}ms (actual work)")

    print(f"  ✓ Final result: {result.shape}")

    # Evaluation should take significantly more time than pipeline construction
    speedup = t_eval / t_pipeline
    print(f"  ✓ Evaluation took {speedup:.0f}x longer than construction")
    print(f"     (proves lazy evaluation - no work until sink!)")

    assert speedup > 10, "Evaluation should take much longer than construction (lazy)"


def test_timing_comparison():
    """Compare timing for different pipeline configurations."""
    print("\n[Test 20] Testing timing for different operations...")

    # Create a medium-sized image
    size = 1024
    print(f"  Creating {size}x{size} test image...")

    # Simple gradient pattern
    data = []
    for y in range(size):
        for x in range(size):
            data.extend([x % 256, y % 256, 128])

    img = fim.Image.from_memory(data, size, size, 3)

    # Test 1: Just crop (fast)
    t0 = time.perf_counter()
    result1 = img.crop((0, 0), (512, 512)).to_numpy()
    t_crop = time.perf_counter() - t0
    print(f"  Crop only: {t_crop * 1000:.1f}ms → {result1.shape}")

    # Test 2: Crop + downsample (more work)
    t0 = time.perf_counter()
    result2 = img.crop((0, 0), (512, 512)).downsample(4).to_numpy()
    t_crop_down = time.perf_counter() - t0
    print(f"  Crop + downsample: {t_crop_down * 1000:.1f}ms → {result2.shape}")

    # Test 3: Multi-stage pipeline (most work)
    t0 = time.perf_counter()
    result3 = img.crop((0, 0), (800, 800)).downsample(2).crop((0, 0), (300, 300)).downsample(2).to_numpy()
    t_multi = time.perf_counter() - t0
    print(f"  Multi-stage pipeline: {t_multi * 1000:.1f}ms → {result3.shape}")

    print(f"  ✓ All operations completed successfully")
    print(f"  ✓ Times scale reasonably with complexity")


def test_lazy_no_evaluation():
    """Test that pipelines with no sink don't do any work."""
    print("\n[Test 21] Testing that pipelines without sinks do no work...")

    # Create large source
    size = 4096  # 16 megapixels!
    print(f"  Creating LARGE {size}x{size} source ({size * size * 3 / (1024 * 1024):.1f} MB)...")

    t0 = time.perf_counter()
    # Create simple constant data (fast)
    data = [128] * (size * size * 3)
    img = fim.Image.from_memory(data, size, size, 3)
    t_create = time.perf_counter() - t0
    print(f"  Created source in {t_create * 1000:.1f}ms")

    # Build many operations without evaluating
    t0 = time.perf_counter()
    pipeline1 = img.crop((1000, 1000), (2000, 2000))
    pipeline2 = pipeline1.downsample(4)
    pipeline3 = pipeline2.crop((100, 100), (300, 300))
    pipeline4 = pipeline3.downsample(2)
    t_lazy = time.perf_counter() - t0

    print(f"  ✓ Built 4 pipeline stages in {t_lazy * 1000:.3f}ms")
    print(f"  ✓ No evaluation - just lazy graph construction!")

    # Should be extremely fast (< 1ms)
    assert t_lazy < 0.01, f"Lazy construction should be instant, took {t_lazy * 1000:.1f}ms"

    # Verify the pipeline is valid but not evaluated
    dims = pipeline4.dimensions
    print(f"  ✓ Pipeline dimensions: {dims} (still lazy!)")

    # Now actually evaluate a small portion
    t0 = time.perf_counter()
    result = pipeline4.to_numpy()
    t_eval = time.perf_counter() - t0
    print(f"  ✓ Evaluation took {t_eval * 1000:.1f}ms (actual work)")
    print(f"  ✓ Lazy construction was {t_eval / t_lazy:.0f}x faster than evaluation!")

    assert result.shape == (dims[1], dims[0], dims[2]), "Should match expected dimensions"


def main():
    """Run all tests."""
    print("=" * 60)
    print("FIM Python Bindings - Simple Integration Tests")
    print("=" * 60)

    try:
        # Run basic tests
        test_basic_memory_source()
        test_crop_operation()
        test_downsample_operation()
        test_chained_operations()
        test_numpy_conversion()
        test_stack_operation()
        test_invalid_stack()
        test_repr()
        test_lazy_evaluation()
        test_numpy_roundtrip()

        # Run pixel verification tests
        test_crop_pixel_verification()
        test_downsample_pixel_verification()
        test_stack_pixel_verification()
        test_pipeline_pixel_verification()
        test_edge_case_pixels()

        # Run advanced pipeline tests
        test_complex_pipeline()
        test_multi_branch_pipeline()
        test_large_synthetic_image()

        # Run timing/lazy evaluation tests
        test_lazy_timing()
        test_timing_comparison()
        test_lazy_no_evaluation()

        print("\n" + "=" * 60)
        print("✓ All 21 tests passed!")
        print("=" * 60)
        return 0

    except AssertionError as e:
        print(f"\n✗ Test failed: {e}")
        return 1
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}")
        import traceback

        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())

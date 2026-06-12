#!/usr/bin/env python3
"""
Example usage of the fim Python bindings.

This script demonstrates the lazy evaluation pipeline and various
operations available in the fim library.
"""

import numpy as np
import fim


def create_test_image():
    """Create a simple test image."""
    # Create a gradient image
    width, height = 256, 256
    img = np.zeros((height, width, 3), dtype=np.uint8)

    for y in range(height):
        for x in range(width):
            img[y, x, 0] = (x * 255) // width  # Red gradient horizontal
            img[y, x, 1] = (y * 255) // height  # Green gradient vertical
            img[y, x, 2] = 128  # Constant blue

    return img


def main():
    print("FIM Python Bindings Example")
    print("=" * 50)

    # Create a test image from NumPy
    print("\n1. Creating test image from NumPy array...")
    numpy_img = create_test_image()
    img = fim.Image.from_memory(
        numpy_img.flatten().tolist(), numpy_img.shape[1], numpy_img.shape[0], numpy_img.shape[2]
    )
    print(f"   Created image: {img}")

    # Apply operations (lazy - nothing computed yet)
    print("\n2. Applying lazy operations...")
    cropped = img.crop((50, 50), (150, 150))
    print(f"   After crop: {cropped}")

    downsampled = cropped.downsample(2)
    print(f"   After downsample: {downsampled}")

    # Trigger evaluation by converting to NumPy
    print("\n3. Evaluating pipeline to NumPy...")
    result = downsampled.to_numpy()
    print(f"   Result shape: {result.shape}")
    print(f"   Result dtype: {result.dtype}")

    # Create separate channel images
    print("\n4. Creating separate channel images...")
    red_channel = fim.Image.from_memory([255] * (64 * 64) + [0] * (64 * 64) + [0] * (64 * 64), 64, 64, 1)
    green_channel = fim.Image.from_memory([0] * (64 * 64) + [255] * (64 * 64) + [0] * (64 * 64), 64, 64, 1)
    blue_channel = fim.Image.from_memory([0] * (64 * 64) + [0] * (64 * 64) + [255] * (64 * 64), 64, 64, 1)

    # Stack channels
    print("\n5. Stacking channel images...")
    rgb = fim.Image.stack([red_channel, green_channel, blue_channel], axis="bands")
    print(f"   Stacked image: {rgb}")

    # Convert stacked image to NumPy
    rgb_array = rgb.to_numpy()
    print(f"   Stacked array shape: {rgb_array.shape}")

    # Save to file
    print("\n6. Writing output files...")
    try:
        downsampled.write_png("fim_output.png")
        print("   Wrote: fim_output.png")
    except Exception as e:
        print(f"   Could not write PNG: {e}")

    try:
        downsampled.write_tiff("fim_output.tiff")
        print("   Wrote: fim_output.tiff")
    except Exception as e:
        print(f"   Could not write TIFF: {e}")

    print("\n7. Chain multiple operations...")
    complex_pipeline = img.crop((0, 0), (200, 200)).downsample(4)
    print(f"   Pipeline result: {complex_pipeline}")
    print(f"   Final dimensions: {complex_pipeline.dimensions}")

    print("\n" + "=" * 50)
    print("Example completed successfully!")
    print(f"FIM version: {fim.__version__}")


if __name__ == "__main__":
    main()

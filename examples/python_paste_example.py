#!/usr/bin/env python3
"""
Example: Black canvas with paste operations for image compositing.

This example demonstrates:
1. Creating a black canvas with deferred channel determination
2. Pasting images at specific positions
3. Chaining multiple paste operations
4. Combining paste with other pipeline operations (crop, resize)
5. Using paste for tile-based image assembly

Requirements:
    pip install numpy pillow
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image as PILImage

# Import fim - the fimage Python bindings
import fim


def create_colored_square(size: int, color: tuple[int, int, int]) -> fim.Image:
    """
    Create a solid-color square image.

    Args:
        size: Size of the square in pixels
        color: RGB color tuple

    Returns:
        fim.Image with the colored square
    """
    data = np.zeros((size, size, 3), dtype=np.uint8)
    data[:, :, 0] = color[0]  # R
    data[:, :, 1] = color[1]  # G
    data[:, :, 2] = color[2]  # B
    return fim.Image.from_numpy(data)


def create_gradient_image(width: int, height: int) -> fim.Image:
    """
    Create a horizontal gradient image.

    Args:
        width: Image width
        height: Image height

    Returns:
        fim.Image with gradient
    """
    data = np.zeros((height, width, 3), dtype=np.uint8)
    for x in range(width):
        value = int(255 * x / width)
        data[:, x, :] = value
    return fim.Image.from_numpy(data)


def example_basic_paste():
    """Basic example: paste colored squares on black canvas."""
    print("\n" + "=" * 60)
    print("EXAMPLE 1: Basic Paste - Colored Squares")
    print("=" * 60)

    # Create colored squares
    red_square = create_colored_square(100, (255, 0, 0))
    green_square = create_colored_square(100, (0, 255, 0))
    blue_square = create_colored_square(100, (0, 0, 255))

    # Create black canvas and paste squares
    canvas = fim.Image.black(500, 500)
    result = canvas.paste(red_square, 50, 50).paste(green_square, 200, 200).paste(blue_square, 350, 350)

    # Save result
    output_path = "paste_colored_squares.png"
    result.write_png(output_path)
    print(f"✓ Created composite with colored squares: {output_path}")
    print(f"  Canvas size: 500x500")
    print(f"  Red square at (50, 50)")
    print(f"  Green square at (200, 200)")
    print(f"  Blue square at (350, 350)")

    return result


def example_overlapping_paste():
    """Example with overlapping pastes - later paste overwrites earlier."""
    print("\n" + "=" * 60)
    print("EXAMPLE 2: Overlapping Pastes")
    print("=" * 60)

    # Create larger colored squares that will overlap
    red_square = create_colored_square(150, (255, 0, 0))
    yellow_square = create_colored_square(150, (255, 255, 0))
    cyan_square = create_colored_square(150, (0, 255, 255))

    # Paste with overlaps
    canvas = fim.Image.black(400, 400)
    result = canvas.paste(red_square, 50, 50).paste(yellow_square, 100, 100).paste(cyan_square, 150, 150)

    # Save result
    output_path = "paste_overlapping.png"
    result.write_png(output_path)
    print(f"✓ Created composite with overlapping squares: {output_path}")
    print(f"  Later pastes overwrite earlier ones in overlapping regions")

    return result


def example_channel_inference():
    """Demonstrate channel inference from pasted image."""
    print("\n" + "=" * 60)
    print("EXAMPLE 3: Channel Inference")
    print("=" * 60)

    # Create grayscale image
    gray_data = np.full((80, 80, 1), 128, dtype=np.uint8)
    gray_img = fim.Image.from_numpy(gray_data)

    # Create RGB image
    rgb_data = np.zeros((80, 80, 3), dtype=np.uint8)
    rgb_data[:, :, 0] = 255  # Red
    rgb_img = fim.Image.from_numpy(rgb_data)

    # Canvas infers channels from first paste
    canvas_gray = fim.Image.black(200, 200)
    result_gray = canvas_gray.paste(gray_img, 60, 60)

    canvas_rgb = fim.Image.black(200, 200)
    result_rgb = canvas_rgb.paste(rgb_img, 60, 60)

    # Save results
    result_gray.write_png("paste_grayscale.png")
    result_rgb.write_png("paste_rgb.png")

    w1, h1, c1 = result_gray.dimensions
    w2, h2, c2 = result_rgb.dimensions

    print(f"✓ Grayscale canvas: {w1}x{h1}, {c1} channel(s)")
    print(f"✓ RGB canvas: {w2}x{h2}, {c2} channel(s)")
    print(f"  Channels automatically inferred from first pasted image!")

    return result_gray, result_rgb


def example_paste_with_pipeline():
    """Combine paste with other pipeline operations."""
    print("\n" + "=" * 60)
    print("EXAMPLE 4: Paste + Pipeline Operations")
    print("=" * 60)

    # Create gradient background
    gradient = create_gradient_image(300, 300)

    # Create colored square
    magenta_square = create_colored_square(100, (255, 0, 255))

    # Paste on black canvas, then crop and resize
    canvas = fim.Image.black(400, 400)
    result = (
        canvas.paste(gradient, 50, 50)
        .paste(magenta_square, 150, 150)
        .crop((25, 25), (350, 350))
        .resize(512, 512, kernel=fim.KernelType.MAGIC2021)
    )

    # Save result
    output_path = "paste_with_pipeline.png"
    result.write_png(output_path)

    w, h, c = result.dimensions
    print(f"✓ Created composite with pipeline: {output_path}")
    print(f"  1. Pasted gradient and square on 400x400 canvas")
    print(f"  2. Cropped to 350x350 region")
    print(f"  3. Resized to {w}x{h} with Magic Kernel 2021")

    return result


def example_tile_assembly():
    """Use paste for tile-based image assembly."""
    print("\n" + "=" * 60)
    print("EXAMPLE 5: Tile-Based Assembly")
    print("=" * 60)

    # Create a checkerboard pattern using paste
    tile_size = 64
    grid_size = 8  # 8x8 grid
    canvas_size = tile_size * grid_size

    # Create black and white tiles
    black_tile = create_colored_square(tile_size, (0, 0, 0))
    white_tile = create_colored_square(tile_size, (255, 255, 255))

    # Start with black canvas
    canvas = fim.Image.black(canvas_size, canvas_size)

    # Paste tiles in checkerboard pattern
    result = canvas
    for row in range(grid_size):
        for col in range(grid_size):
            # Checkerboard: alternate black and white
            is_white = (row + col) % 2 == 0
            if is_white:
                x = col * tile_size
                y = row * tile_size
                result = result.paste(white_tile, x, y)

    # Save result
    output_path = "paste_checkerboard.png"
    result.write_png(output_path)
    print(f"✓ Created {grid_size}x{grid_size} checkerboard: {output_path}")
    print(f"  Canvas size: {canvas_size}x{canvas_size}")
    print(f"  Tile size: {tile_size}x{tile_size}")
    print(f"  Total tiles pasted: {grid_size * grid_size // 2}")

    return result


def example_lazy_evaluation():
    """Demonstrate that paste is lazy - no computation until sink."""
    print("\n" + "=" * 60)
    print("EXAMPLE 6: Lazy Evaluation")
    print("=" * 60)

    import time

    # Create a very large canvas
    large_size = 10000
    canvas = fim.Image.black(large_size, large_size)

    # Create small images to paste
    red_square = create_colored_square(200, (255, 0, 0))
    blue_square = create_colored_square(200, (0, 0, 255))

    # Measure paste operations (should be instant - lazy!)
    start = time.perf_counter()
    result = (
        canvas.paste(red_square, 1000, 1000)
        .paste(blue_square, 5000, 5000)
        .crop((800, 800), (3000, 3000))  # Crop to manageable size
        .resize(512, 512)
    )
    paste_time = time.perf_counter() - start

    print(f"✓ Pipeline construction (lazy): {paste_time * 1000:.2f}ms")
    print(f"  Created {large_size}x{large_size} canvas (instantly!)")
    print(f"  Pasted 2 images (lazy - no actual work done)")
    print(f"  Cropped and resized (lazy - building computation graph)")

    # Measure evaluation (triggers actual computation)
    start = time.perf_counter()
    result.write_png("paste_lazy_demo.png")
    eval_time = time.perf_counter() - start

    print(f"✓ Pipeline evaluation: {eval_time * 1000:.1f}ms")
    print(f"  Actual computation happens only when saving!")
    print(f"  Speedup from lazy evaluation: {eval_time / paste_time:.0f}x")

    return result


def example_from_file(image_path: str):
    """Example using actual image files if provided."""
    print("\n" + "=" * 60)
    print("EXAMPLE 7: Paste Real Images")
    print("=" * 60)

    if not Path(image_path).exists():
        print(f"⚠ Image file not found: {image_path}")
        print("  Skipping this example")
        return None

    # Load image with fim
    img = fim.Image.from_png(image_path) if image_path.endswith(".png") else fim.Image.from_libtiff(image_path, page=0)

    # Get dimensions
    w, h, c = img.dimensions
    print(f"✓ Loaded image: {w}x{h}, {c} channels")

    # Create larger canvas and paste image multiple times
    canvas_size = max(w, h) * 3
    canvas = fim.Image.black(canvas_size, canvas_size)

    # Paste in 3x3 grid
    result = canvas
    for row in range(3):
        for col in range(3):
            x = col * w
            y = row * h
            if x + w <= canvas_size and y + h <= canvas_size:
                result = result.paste(img, x, y)

    # Crop and resize
    result = result.crop((0, 0), (canvas_size, canvas_size)).resize(1024, 1024)

    # Save
    output_path = "paste_real_image_grid.png"
    result.write_png(output_path)
    print(f"✓ Created 3x3 grid: {output_path}")

    return result


def main():
    """Main example entry point."""
    print("\n" + "=" * 60)
    print("FIM PASTE OPERATION EXAMPLES")
    print("=" * 60)
    print("\nThese examples demonstrate the black canvas and paste functionality:")
    print("- Black canvas with lazy channel inference")
    print("- Paste operations at arbitrary positions")
    print("- Chaining multiple pastes")
    print("- Combining paste with crop, resize, etc.")
    print("- Lazy evaluation for performance")

    # Run examples
    example_basic_paste()
    example_overlapping_paste()
    example_channel_inference()
    example_paste_with_pipeline()
    example_tile_assembly()
    example_lazy_evaluation()

    # If user provides an image file, use it
    if len(sys.argv) > 1:
        example_from_file(sys.argv[1])

    print("\n" + "=" * 60)
    print("ALL EXAMPLES COMPLETE!")
    print("=" * 60)
    print("\nCheck the generated PNG files to see the results.")
    print("\nUsage: python python_paste_example.py [optional_image_path.png]")


if __name__ == "__main__":
    main()

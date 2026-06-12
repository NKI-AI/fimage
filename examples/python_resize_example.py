#!/usr/bin/env python3
"""
Example: High-quality image resizing with fim and fastslide.

This example demonstrates:
1. Loading a whole slide image with fastslide
2. Extracting and printing metadata
3. Cropping the center region
4. Rescaling with Magic Kernel 2021 for highest quality
5. Converting to PIL Image for further processing

Requirements:
    pip install numpy pillow
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image as PILImage

# Import fim - the fimage Python bindings
import fim


def load_and_process_slide(slide_path: str, output_size: int = 512) -> PILImage.Image:
    """
    Load a whole slide image, crop center, resize, and convert to PIL.

    Args:
        slide_path: Path to the whole slide image file (SVS, MRXS, QPTIFF, etc.)
        output_size: Target output size (width and height) in pixels

    Returns:
        PIL Image with the processed region
    """
    print(f"Loading slide: {slide_path}")

    # Step 1: Load image with fastslide using context manager
    with fim.open_fastslide(slide_path) as slide:
        # Step 2: Print metadata
        print(f"\n=== Slide Metadata ===")
        print(f"Format: {slide.format_name}")
        print(f"Pyramid levels: {slide.level_count}")
        print(f"MPP: {slide.mpp}")

        # Show all level dimensions
        channels = slide.num_channels
        for level in range(slide.level_count):
            width, height = slide.level_dimensions[level]
            downsample = slide.level_downsamples[level]
            print(f"  Level {level}: ({width}, {height}, {channels}) (downsample: {downsample:.2f}x)")

        # Get level 0 image
        img = slide.at_level(0)
        width, height, channels = img.dimensions
        print(f"\nWorking with Level 0: {width} x {height} pixels, {channels} channels")
        print(f"Estimated size: {(width * height * channels) / (1024**2):.1f} MB")

    # Step 3: Crop the center region
    # Calculate center crop coordinates
    crop_size = min(width, height) // 2  # Use half of the smallest dimension
    crop_x = (width - crop_size) // 2
    crop_y = (height - crop_size) // 2

    print(f"\n=== Cropping ===")
    print(f"Crop region: ({crop_x}, {crop_y}) -> ({crop_x + crop_size}, {crop_y + crop_size})")
    print(f"Crop size: {crop_size} x {crop_size}")

    # Apply center crop (lazy - no computation yet)
    cropped = img.crop((crop_x, crop_y), (crop_size, crop_size))

    # Step 4: Rescale with Magic Kernel 2021
    # This kernel provides the highest quality with minimal ringing artifacts
    print(f"\n=== Resizing ===")
    print(f"Target size: {output_size} x {output_size}")
    print(f"Kernel: Magic Kernel Sharp 2021")
    print(f"Scale factor: {output_size / crop_size:.3f}x")

    # Apply resize with Magic2021 kernel (lazy - still no computation)
    resized = cropped.resize(output_size, output_size, kernel=fim.KernelType.MAGIC2021)

    # Step 5: Convert to NumPy array
    # This triggers evaluation of the entire pipeline (load -> crop -> resize)
    print(f"\n=== Evaluating Pipeline ===")
    print("Triggering computation (crop + resize)...")
    array = resized.to_numpy()

    print(f"Output array shape: {array.shape}")
    print(f"Output array dtype: {array.dtype}")
    print(f"Value range: [{array.min()}, {array.max()}]")

    # Convert NumPy array to PIL Image
    pil_image = PILImage.fromarray(array)

    print(f"\n=== PIL Image ===")
    print(f"PIL mode: {pil_image.mode}")
    print(f"PIL size: {pil_image.size}")

    return pil_image


def advanced_example_with_box(slide_path: str) -> PILImage.Image:
    """
    Advanced example using subpixel-accurate box parameter.

    This demonstrates precise region selection combined with resizing,
    which is useful for avoiding rounding errors in coordinate transformations.
    """
    print(f"\n{'=' * 60}")
    print("ADVANCED EXAMPLE: Subpixel Box Parameter")
    print(f"{'=' * 60}\n")

    img = fim.Image.from_fastslide(slide_path, level=0)
    width, height, channels = img.dimensions

    # Define a precise region with subpixel coordinates
    # This is useful when you need exact alignment, e.g., for tile registration
    box = fim.Box(
        100.25,  # x1: left edge at pixel 100.25
        200.75,  # y1: top edge at pixel 200.75
        600.25,  # x2: right edge at pixel 600.25
        700.75,  # y2: bottom edge at pixel 700.75
    )

    print(f"Subpixel box: ({box.x1}, {box.y1}) -> ({box.x2}, {box.y2})")
    print(f"Box dimensions: {box.width()} x {box.height()}")

    # Resize with box parameter
    # This extracts and resizes the precise region in one step
    resized = img.resize(256, 256, kernel=fim.KernelType.MAGIC2021, box=box)

    array = resized.to_numpy()
    pil_image = PILImage.fromarray(array)

    print(f"Output: {array.shape}")

    return pil_image


def compare_kernels(slide_path: str, output_size: int = 512):
    """
    Compare different resampling kernels visually.

    Creates three versions using different kernels to demonstrate quality differences.
    """
    print(f"\n{'=' * 60}")
    print("KERNEL COMPARISON")
    print(f"{'=' * 60}\n")

    img = fim.Image.from_fastslide(slide_path, level=0)
    width, height, channels = img.dimensions

    # Crop center region
    crop_size = min(width, height) // 2
    crop_x = (width - crop_size) // 2
    crop_y = (height - crop_size) // 2
    cropped = img.crop((crop_x, crop_y), (crop_size, crop_size))

    # Test all three kernels
    kernels = [
        (fim.KernelType.LANCZOS2, "Lanczos2 (fastest)"),
        (fim.KernelType.LANCZOS3, "Lanczos3 (default)"),
        (fim.KernelType.MAGIC2021, "Magic2021 (highest quality)"),
    ]

    results = {}
    for kernel, name in kernels:
        print(f"Processing with {name}...")
        resized = cropped.resize(output_size, output_size, kernel=kernel)
        array = resized.to_numpy()
        results[name] = PILImage.fromarray(array)

    print(f"\nGenerated {len(results)} versions")
    return results


def example_paste_tiles(slide_path: str) -> PILImage.Image:
    """
    Example demonstrating paste functionality with tiles from a slide.

    This shows how to extract a region from a whole slide image, resize it,
    and paste it onto a black canvas at specific positions.
    """
    print(f"\n{'=' * 60}")
    print("EXAMPLE: Paste Resized Tile onto Black Canvas")
    print(f"{'=' * 60}\n")

    print(f"Loading slide: {slide_path}")

    # Load the slide and extract a specific region
    with fim.open_fastslide(slide_path) as slide:
        level0 = slide.at_level(0)
        w, h, c = level0.dimensions
        print(f"Slide dimensions: {w}x{h}, {c} channels")

        # Extract region from (1600, 1600) with size (3200, 3200)
        extract_x, extract_y = 1600, 1600
        extract_w, extract_h = 3200, 3200

        print(f"\nExtracting region: ({extract_x}, {extract_y}) with size ({extract_w}, {extract_h})...")

        # Crop and resize to 400x400 in one lazy pipeline
        tile_resized = level0.crop((extract_x, extract_y), (extract_w, extract_h)).resize(
            400, 400, kernel=fim.KernelType.MAGIC2021
        )

        # Verify dimensions
        tile_w, tile_h, tile_c = tile_resized.dimensions
        print(f"  After resize: {tile_w}x{tile_h}, {tile_c} channels")
        assert tile_w == 400 and tile_h == 400, "Tile size mismatch!"

    # Create a black canvas (2048 x 2048)
    canvas_size = 2048
    print(f"\nCreating {canvas_size}x{canvas_size} black canvas...")
    canvas = fim.Image.black(canvas_size, canvas_size)
    print("  Canvas created (deferred - no memory allocated yet!)")

    # Paste the resized tile at three different positions
    location1 = (100, 100)
    location2 = (700, 700)
    location3 = (1300, 1300)

    print(f"\nPasting resized tile at multiple locations (all lazy):")
    print(f"  Location 1: {location1}")
    print(f"  Location 2: {location2}")
    print(f"  Location 3: {location3}")

    result = (
        canvas.paste(tile_resized, location1[0], location1[1])
        .paste(tile_resized, location2[0], location2[1])
        .paste(tile_resized, location3[0], location3[1])
    )

    print("  ✓ Pasted 3 tiles (lazy - no actual computation yet)")

    # Apply some post-processing to the composite
    print(f"\nApplying post-processing (resize to 1024x1024, still lazy)...")
    processed = result.resize(1024, 1024, kernel=fim.KernelType.MAGIC2021)

    # This triggers evaluation of the entire pipeline
    print(f"\nEvaluating pipeline...")
    array = processed.to_numpy()
    print(f"  Output array shape: {array.shape}")

    # Convert to PIL
    pil_image = PILImage.fromarray(array)

    print(f"\n✓ Pipeline evaluation complete!")
    print(f"  - Extracted region (1600, 1600) size (3200, 3200) from slide")
    print(f"  - Resized to 400x400")
    print(f"  - Pasted onto black canvas at 3 locations")
    print(f"  - Resized final composite to {array.shape[1]}x{array.shape[0]}")

    return pil_image


def main():
    """Main example entry point."""
    # Check if slide path is provided
    if len(sys.argv) < 2:
        print("Usage: python python_resize_example.py <slide_path> [output_size]")
        print("\nExample:")
        print("  python python_resize_example.py slide.svs 512")
        print("  python python_resize_example.py slide.mrxs 1024")
        sys.exit(1)

    slide_path = sys.argv[1]
    output_size = int(sys.argv[2]) if len(sys.argv) > 2 else 512

    # Verify file exists
    if not Path(slide_path).exists():
        print(f"Error: File not found: {slide_path}")
        sys.exit(1)

    print(f"\n{'=' * 60}")
    print("FIM + FASTSLIDE: High-Quality Image Resizing Example")
    print(f"{'=' * 60}\n")

    # Basic example
    pil_image = load_and_process_slide(slide_path, output_size)

    # Save result
    output_path = f"output_{output_size}x{output_size}.png"
    pil_image.save(output_path)
    print(f"\nSaved to: {output_path}")

    # Advanced example with box parameter
    pil_box = advanced_example_with_box(slide_path)
    pil_box.save("output_with_box.png")
    print(f"Saved to: output_with_box.png")

    # Compare kernels
    results = compare_kernels(slide_path, output_size=256)
    for name, img_result in results.items():
        filename = f"output_{name.replace(' ', '_').lower()}.png"
        img_result.save(filename)
        print(f"Saved {name}: {filename}")

    # Paste example - create a composite from multiple tiles
    pil_paste = example_paste_tiles(slide_path)
    pil_paste.save("output_paste_composite.png")
    print(f"Saved paste composite: output_paste_composite.png")

    print(f"\n{'=' * 60}")
    print("Processing complete!")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Simple example: Load with fastslide, crop, resize, convert to PIL.

This is a minimal example showing the complete workflow with context manager.
"""

import fim
import numpy as np
from PIL import Image as PILImage


def process_slide(slide_path: str, output_size: int = 512) -> PILImage.Image:
    """Process a slide: load, crop center, resize, convert to PIL."""

    # 1) Load image with fastslide using context manager
    with fim.open_fastslide(slide_path) as slide:
        # Print slide metadata
        print(f"Format: {slide.format_name}")
        print(f"Pyramid levels: {slide.level_count}")
        print(f"MPP: {slide.mpp}")

        # Get image at level 0 (highest resolution)
        img = slide.at_level(0)

        # 2) Print metadata
        width, height, channels = img.dimensions
        print(f"Level 0: {width}x{height}, {channels} channels")
        print(f"Size: {(width * height * channels) / (1024**2):.1f} MB")

        # 3) Crop the center
        crop_size = min(width, height) // 2
        crop_x = (width - crop_size) // 2
        crop_y = (height - crop_size) // 2
        cropped = img.crop((crop_x, crop_y), (crop_size, crop_size))
        print(f"Cropped to: {crop_size}x{crop_size}")

        # 4) Rescale with MagicKernel2021
        resized = cropped.resize(output_size, output_size, kernel=fim.KernelType.MAGIC2021)
        print(f"Resized to: {output_size}x{output_size}")

        # 5) Convert to PIL Image
        array = resized.to_numpy()  # Triggers evaluation
        pil_image = PILImage.fromarray(array)
        print(f"PIL Image: mode={pil_image.mode}, size={pil_image.size}")

        return pil_image


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python simple_resize_example.py <slide_path>")
        sys.exit(1)

    # Process slide
    pil_img = process_slide(sys.argv[1], output_size=512)

    # Save result
    pil_img.save("output.png")
    print("\nSaved to: output.png")

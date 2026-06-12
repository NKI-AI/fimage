"""
FIM: Fast Image processing library with lazy evaluation.

This module provides a PyVips-style lazy image processing pipeline built on
top of a high-performance C++ backend using CRTP for zero-overhead abstraction.

Example:
    >>> import fim
    >>>
    >>> # Load and process an image lazily
    >>> img = fim.Image.from_libtiff("input.tiff")
    >>> result = img.crop((100, 100), (512, 512)).downsample(2)
    >>>
    >>> # Nothing has been computed yet - evaluation happens on write
    >>> result.write_png("output.png")
    >>>
    >>> # Or convert to numpy
    >>> array = result.to_numpy()  # shape: (height, width, channels)
    >>>
    >>> # High-quality resize with different kernels
    >>> resized = img.resize(512, 512, kernel=fim.KernelType.LANCZOS3)
    >>>
    >>> # Resize with subpixel-accurate box parameter
    >>> box = fim.Box(10.5, 20.5, 510.5, 520.5)
    >>> cropped_resized = img.resize(256, 256, box=box)
    >>>
    >>> # Stack multiple images
    >>> red = fim.Image.from_png("red.png")
    >>> green = fim.Image.from_png("green.png")
    >>> blue = fim.Image.from_png("blue.png")
    >>> rgb = fim.Image.stack([red, green, blue], axis="bands")
    >>> rgb.write_tiff("rgb.tiff")
"""

from ._fim import (
    Image,
    KernelType,
    PixelType,
    DataLayout,
    CompressionType,
    Box,
    DeferredBlackCanvas,
    FastSlideContext,
    open_fastslide,
    LibtiffContext,
    open_libtiff,
    __version__,
)

try:
    from ._fim import OpenSlideContext, open_openslide  # type: ignore[attr-defined]
except ImportError:
    OpenSlideContext = None  # type: ignore[assignment]
    open_openslide = None  # type: ignore[assignment]

__all__ = [
    "Image",
    "KernelType",
    "PixelType",
    "DataLayout",
    "CompressionType",
    "Box",
    "DeferredBlackCanvas",
    "FastSlideContext",
    "open_fastslide",
    "LibtiffContext",
    "open_libtiff",
    "__version__",
]

if OpenSlideContext is not None and open_openslide is not None:
    __all__.extend(["OpenSlideContext", "open_openslide"])

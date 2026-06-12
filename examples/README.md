# FIM Examples

This directory contains examples demonstrating the FIM (FastImage) library's capabilities.

## Python Examples

### simple_resize_example.py

A minimal example showing the complete workflow requested:

1. Load image with fastslide
2. Print metadata
3. Crop the center
4. Rescale with MagicKernel2021
5. Convert to PIL Image

**Usage:**

```bash
python simple_resize_example.py slide.svs
```

**Output:**

- Prints slide dimensions and metadata
- Saves `output.png` with the processed result

### python_resize_example.py

A comprehensive example demonstrating:

- Loading whole slide images with fastslide integration
- Metadata extraction and display
- Center cropping
- High-quality resizing with different kernels
- Subpixel-accurate box parameter usage
- Kernel comparison (Lanczos2 vs Lanczos3 vs Magic2021)
- Converting to PIL Image for further processing

**Usage:**

```bash
python python_resize_example.py slide.svs 512
python python_resize_example.py slide.mrxs 1024
```

**Outputs:**

- `output_512x512.png` - Main result
- `output_with_box.png` - Subpixel box example
- `output_lanczos2_(fastest).png` - Lanczos2 kernel result
- `output_lanczos3_(default).png` - Lanczos3 kernel result
- `output_magic2021_(highest_quality).png` - Magic2021 kernel result

## Key Concepts

### Lazy Evaluation

All FIM operations are lazy - they build a computation graph but don't execute until you:

- Call `.to_numpy()` to convert to NumPy array
- Call `.write_png()` or `.write_tiff()` to save to file

This allows efficient processing of large images by only computing what's needed.

### Resampling Kernels

- **LANCZOS2**: Fastest, radius 2, good quality
- **LANCZOS3**: Default, radius 3, excellent quality/speed balance
- **MAGIC2021**: Highest quality, radius 4.5, minimal ringing artifacts

### Box Parameter

The `Box` class allows specifying source regions with floating-point pixel coordinates:

```python
box = fim.Box(x1=10.5, y1=20.5, x2=510.5, y2=520.5)
```

This is useful for:

- Subpixel shifts (like MRXS tile alignment)
- Precise region extraction combined with scaling
- Avoiding rounding errors in coordinate transformations

## Requirements

```bash
pip install numpy pillow
```

## Building and Running

If you've built FIM with Bazel, the Python bindings are available in the workspace:

```bash
# Build the Python bindings
bazelisk build //aifo/fimage/python:fim

# Run tests to verify installation
bazelisk test //aifo/fimage/src/python:resize_test

# Run the example
python3 examples/simple_resize_example.py /path/to/slide.svs
```

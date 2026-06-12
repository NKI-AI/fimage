# fim (FastImage)

A high-performance C++20 library for tile-based image processing using CRTP
(Curiously Recurring Template Pattern), with Python bindings that provide a
PyVips-style lazy-evaluation pipeline.

This repository is automatically synced from the AI for Oncology monorepo. It
builds both with [Bazel](https://bazel.build/) (via bzlmod) and with
[Meson](https://mesonbuild.com/) (used to produce the Python wheels).

## Features

- **Tile-based processing**: efficient processing of large images through tiling
- **CRTP pipeline**: zero-overhead composition using compile-time polymorphism
- **Lazy evaluation**: operations are deferred until a sink materializes the result
- **Multiple sources**: TIFF (libtiff), PNG (lodepng/libspng), and FastSlide (whole slide images)
- **Operators**: crop, paste, downsample, resize (Lanczos / Magic2021 kernels), and stack
- **Flexible sinks**: tiled TIFF/PNG output or zero-copy NumPy arrays
- **Python bindings**: full Python API via nanobind

## Installation (Python)

```bash
pip install fimage-python
```

```python
import fim

img = fim.Image.from_libtiff("input.tiff")
img.crop(100, 100, 512, 512).resize(256, 256).write_png("output.png")
```

## Building from source

### Bazel

```bash
# Build the C++ library
bazelisk build //:fimage

# Build the Python extension
bazelisk build //python:fim

# Run the test suite
bazelisk test //...
```

### Meson (wheels)

```bash
# Build a wheel + sdist for the current interpreter
uv build
```

The Meson build fetches every dependency through `subprojects/*.wrap` and links
them statically, producing a self-contained `_fim` extension.

## License

Apache 2.0 - see [LICENSE](LICENSE) for details.

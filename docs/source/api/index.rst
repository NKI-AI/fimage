API Reference
=============

This section provides comprehensive documentation for both the C++ and Python APIs.

.. toctree::
   :maxdepth: 3
   :caption: API Documentation:

   python_api
   cpp_api

Overview
--------

fimage provides two complementary APIs:

- **Python API**: High-level interface with NumPy integration, perfect for research and prototyping
- **C++ API**: High-performance native interface for production applications

The APIs are designed to be consistent - most operations are available in both languages with similar signatures.

Key Concepts
~~~~~~~~~~~~

**Image Pipeline**
   Chain of processing operations that transform image data from source to sink.

**Lazy Evaluation**
   Operations build a computation graph but don't execute until a sink materializes the result.

**Tiles**
   Rectangular regions of image data processed independently for efficiency.

**CRTP (C++ only)**
   Curiously Recurring Template Pattern for zero-overhead abstraction.

**Type Erasure (Python)**
   Pattern that hides C++ template complexity while preserving lazy evaluation.

Performance Notes
~~~~~~~~~~~~~~~~~

- Use tile-based processing for large images to minimize memory usage
- The C++ API provides zero-overhead operations with compile-time optimization
- Python API maintains lazy evaluation and uses zero-copy NumPy conversion
- Both APIs support multi-threaded tile processing
- Move semantics (C++) or Python's reference counting manage memory efficiently

API Comparison
~~~~~~~~~~~~~~

.. list-table:: Feature Comparison
   :widths: 30 35 35
   :header-rows: 1

   * - Feature
     - C++ API
     - Python API
   * - **Image Loading**
     - `CreateTiffImage()`, `CreatePngImage()`
     - `Image.from_libtiff()`, `Image.from_png()`
   * - **Cropping**
     - `.Crop(x, y, w, h)`
     - `.crop(x, y, w, h)`
   * - **Downsampling**
     - `.Downsample(factor)`
     - `.downsample(factor)`
   * - **Stacking**
     - `Stack` operator
     - `Image.stack([...])`
   * - **Rendering**
     - `.Render(TiffSink())`, `.Render(LodePngSink())`
     - `.write_tiff()`, `.write_png()`
   * - **NumPy Conversion**
     - N/A
     - `.to_numpy()`
   * - **Type Safety**
     - Compile-time
     - Runtime
   * - **Performance**
     - Zero overhead
     - Near-native with Python overhead

Quick Examples
~~~~~~~~~~~~~~

**C++ Basic Pipeline:**

.. code-block:: cpp

   #include <fim/image.h>
   
   // Load, crop, downsample, and save
   fim::CreateTiffImage("input.tiff")
       .Crop(100, 100, 512, 512)
       .Downsample(2)
       .Render(fim::LodePngSink("output.png"));

**Python Basic Pipeline:**

.. code-block:: python

   import fim
   
   # Load, crop, downsample, and save
   img = fim.Image.from_libtiff("input.tiff")
   result = img.crop(100, 100, 512, 512).downsample(2)
   result.write_png("output.png")

**Python NumPy Integration:**

.. code-block:: python

   import fim
   import numpy as np
   
   # Load image and convert to NumPy
   img = fim.Image.from_libtiff("input.tiff")
   array = img.to_numpy()  # Zero-copy when possible
   
   # Process with NumPy
   processed = array * 0.5
   
   # Create new image from NumPy array
   result = fim.Image.from_memory(
       processed.flatten().tolist(),
       array.shape[1],  # width
       array.shape[0],  # height
       array.shape[2]   # channels
   )


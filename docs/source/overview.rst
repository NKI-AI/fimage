Overview
========

fimage (FastImage) is a minimal, high-performance C++20 library for tile-based image processing. 
It provides both C++ and Python APIs with a focus on zero-overhead abstraction, lazy evaluation, 
and ease of use.

.. raw:: html

   <div style="text-align: center; margin: 20px 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border-radius: 10px; color: white;">
       <h2 style="margin: 0; color: white;">🎨 Tile-based Image Processing</h2>
       <p style="margin: 10px 0 0 0; color: white;">Zero-overhead • CRTP • Lazy Evaluation</p>
   </div>

Key Features
------------

🚀 **Performance**
   - **Zero-overhead CRTP**: Compile-time polymorphism eliminates virtual function calls
   - **Lazy evaluation**: Build computation graphs, execute only when needed
   - **Multi-threaded processing**: Parallel tile processing via thread pool
   - **Memory efficient**: Tile-based approach minimizes memory footprint

🔒 **Memory Safety**
   - Modern C++20 with RAII patterns
   - Value semantics with move optimization
   - No raw pointers or manual memory management
   - Smart pointer usage throughout

🌐 **Cross-Platform**
   - macOS and Linux support
   - x86_64 and ARM64 architectures
   - Bazel build system
   - Consistent behavior across platforms

📁 **Format Support**
   - **TIFF**: Read and write tiled/strip TIFF files (via libtiff)
   - **PNG**: Read and write PNG files (via lodepng)
   - **Memory**: Create images from in-memory buffers
   - **Extensible**: Easy to add new formats

🐍 **Dual APIs**
   - **C++ API**: Native high-performance interface with compile-time guarantees
   - **Python API**: NumPy-integrated bindings with lazy evaluation
   - **Consistent design**: Similar operations in both languages

Design Principles
-----------------

1. **Zero-overhead Abstraction**
   
   fimage uses CRTP (Curiously Recurring Template Pattern) to achieve compile-time 
   polymorphism without runtime overhead. The compiler can inline and optimize 
   the entire pipeline.

   .. code-block:: cpp

      // All type information resolved at compile time
      fim::Image<fim::TiffSource>("input.tiff")
          .Crop(0, 0, 512, 512)      // Image<Crop<TiffSource>>
          .Downsample(2)             // Image<Downsample<Crop<TiffSource>>>
          .Render(fim::LodePngSink("output.png"));

2. **Lazy Evaluation**
   
   Operations build a computation graph but don't execute until a sink materializes 
   the result. This allows:
   
   - Pipeline optimization
   - Avoiding unnecessary computation
   - Memory efficiency
   - Natural parallelization

   .. code-block:: python

      # Nothing computed yet - just building the graph
      img = fim.Image.from_libtiff("input.tiff")
      cropped = img.crop(0, 0, 1024, 1024)
      downsampled = cropped.downsample(2)
      
      # Computation happens here
      downsampled.write_png("output.png")

3. **Tile-based Processing**
   
   Images are processed in rectangular tiles, which:
   
   - Enables processing of images larger than memory
   - Facilitates parallel processing
   - Improves cache locality
   - Matches file format internals (e.g., tiled TIFFs)

4. **Composable Pipeline**
   
   Operators can be chained in any order to build complex processing pipelines:

   .. code-block:: cpp

      source.Crop(...).Downsample(...).Crop(...).Render(sink);

5. **Type Safety (C++)**
   
   The C++ API uses strong typing and compile-time checks to prevent errors:
   
   - Type mismatches caught at compile time
   - No runtime type checks needed
   - Template metaprogramming for validation

6. **Type Erasure (Python)**
   
   Python bindings use type erasure to provide a clean API while maintaining 
   the CRTP pipeline internally:
   
   - Single `Image` class in Python
   - Internal CRTP pipeline preserved
   - Lazy evaluation maintained

7. **Extensibility**
   
   Easy to add new components:
   
   - Sources: Inherit from `SourceBase<Derived>`
   - Operators: Inherit from `OperatorBase<Derived, InputType>`
   - Sinks: Inherit from `SinkBase<Derived>`

Use Cases
---------

Image Processing Pipelines
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   
   # Process a whole-slide pathology image
   slide = fim.Image.from_libtiff("slide.tiff")
   
   # Extract a patch at full resolution
   patch = slide.crop(10000, 12000, 2048, 2048)
   
   # Create thumbnail
   thumbnail = patch.downsample(8)
   thumbnail.write_png("thumbnail.png")

Deep Learning Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   import torch
   from torch.utils.data import Dataset
   
   class TileDataset(Dataset):
       def __init__(self, image_path, tile_size=224, stride=224):
           self.image = fim.Image.from_libtiff(image_path)
           dims = self.image.dimensions
           
           # Generate tile positions
           self.positions = []
           for y in range(0, dims[1], stride):
               for x in range(0, dims[0], stride):
                   self.positions.append((x, y))
           
           self.tile_size = tile_size
       
       def __len__(self):
           return len(self.positions)
       
       def __getitem__(self, idx):
           x, y = self.positions[idx]
           
           # Lazy evaluation: tile extracted only when accessed
           tile = self.image.crop(x, y, self.tile_size, self.tile_size)
           array = tile.to_numpy()
           
           # Convert to PyTorch tensor
           tensor = torch.from_numpy(array).permute(2, 0, 1).float() / 255.0
           return tensor

Multi-Channel Image Composition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   
   # Load separate channel images
   red = fim.Image.from_png("red_channel.png")
   green = fim.Image.from_png("green_channel.png")
   blue = fim.Image.from_png("blue_channel.png")
   
   # Stack along channel axis
   rgb = fim.Image.stack([red, green, blue], axis="bands")
   
   # Process composite image
   result = rgb.crop(0, 0, 1024, 1024).downsample(2)
   result.write_tiff("rgb_result.tiff")

C++ vs Python API Comparison
-----------------------------

.. list-table:: API Comparison
   :widths: 30 35 35
   :header-rows: 1

   * - Feature
     - C++ API
     - Python API
   * - **Lazy Evaluation**
     - ✅ Compile-time
     - ✅ Runtime
   * - **Type Safety**
     - ✅ Compile-time checks
     - ⚠️ Runtime checks
   * - **Performance**
     - ⭐⭐⭐ Zero overhead
     - ⭐⭐ Native speed with Python overhead
   * - **Memory Management**
     - ✅ RAII, move semantics
     - ✅ Automatic (Python GC)
   * - **Pipeline Chaining**
     - ✅ Fluent API
     - ✅ Fluent API
   * - **NumPy Integration**
     - ❌ N/A
     - ✅ Zero-copy conversion
   * - **Error Handling**
     - Exceptions
     - Python exceptions
   * - **Extensibility**
     - ✅ Template-based
     - ⚠️ Via C++ extension

Performance Characteristics
----------------------------

**Memory Usage**
   - Tile-based processing: O(tile_size), not O(image_size)
   - Move semantics minimize copying
   - Lazy evaluation avoids intermediate allocations

**Computation**
   - CRTP: No virtual function overhead
   - Compiler inlining and optimization
   - Multi-threaded tile processing

**I/O**
   - Efficient TIFF tile reading (matches file format)
   - Streaming PNG decoding
   - Minimal memory allocations

Next Steps
----------

- :doc:`architecture` - Deep dive into CRTP pipeline and type erasure
- :doc:`api/index` - Detailed API documentation
- :doc:`guides/index` - Tutorials for extending the library
- :doc:`examples/index` - Complete working examples


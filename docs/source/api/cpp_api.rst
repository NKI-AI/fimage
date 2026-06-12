C++ API Reference
==================

The fimage C++ API provides high-performance, zero-overhead access to tile-based image 
processing using modern C++20 features and the CRTP pattern.

.. note::
   **📊 Doxygen Documentation Available**
   
   For the complete C++ API reference with interactive dependency graphs, inheritance diagrams, 
   and detailed call/caller graphs, see the `Doxygen Documentation <../doxygen/index.html>`_.
   
   This page provides an overview with code examples. The Doxygen documentation
   offers comprehensive C++ details with beautiful SVG visualizations.

.. cpp:namespace:: fim

Core Classes
------------

Image<SourceType>
~~~~~~~~~~~~~~~~~

.. doxygenclass:: fim::Image
   :members:

The main image class template that wraps pipeline stages and provides a fluent 
interface for building processing pipelines.

**Template Parameter:**

- `SourceType`: The underlying source or operator type

**Example Usage:**

.. code-block:: cpp

   #include <fim/image.h>
   
   // Create image from TIFF file
   auto image = fim::CreateTiffImage("input.tiff");
   
   // Chain operations (each returns new Image with different SourceType)
   auto result = std::move(image)
       .Crop(100, 100, 512, 512)      // Image<Crop<TiffSource>>
       .Resize(256, 256)              // Image<Resize<Crop<TiffSource>>>
       .Downsample(2);                // Image<Downsample<Resize<Crop<TiffSource>>>>
   
   // Render to sink
   result.Render(fim::LodePngSink("output.png"));

**Methods:**

- `Crop(x, y, width, height) &&`: Apply crop operation (consumes image)
- `Downsample(factor) &&`: Apply downsample operation (consumes image)
- `Resize(width, height, kernel, box) &&`: Apply resize operation (consumes image)
- `Render(sink)`: Materialize pipeline using sink
- `GetSource()`: Access underlying source/operator

Base Classes
------------

SourceBase<Derived>
~~~~~~~~~~~~~~~~~~~

.. doxygenclass:: fim::SourceBase
   :members:
   :protected-members:

CRTP base class for all image sources. Sources provide image data from files or memory.

**Required Methods in Derived Class:**

.. code-block:: cpp

   class MySource : public SourceBase<MySource> {
   public:
       // Return image dimensions
       ImageInfo GetDimensions() const;
       
       // Return a rectangular tile of data
       Tile GetTile(int x, int y, int width, int height) const;
       
       // Suggest optimal tile size for this source
       TileSize GetIdealTileSize() const;
   };

**Thread Safety:**

All source implementations must be thread-safe for concurrent tile reads.

OperatorBase<Derived, InputType>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenclass:: fim::OperatorBase
   :members:
   :protected-members:

CRTP base class for all image operators. Operators transform data from an input stage.

**Template Parameters:**

- `Derived`: The derived operator class
- `InputType`: The type of the input stage

**Required Methods in Derived Class:**

.. code-block:: cpp

   template <typename InputType>
   class MyOperator : public OperatorBase<MyOperator<InputType>, InputType> {
   public:
       explicit MyOperator(InputType input)
           : OperatorBase<MyOperator, InputType>(std::move(input)) {}
       
       // Return output dimensions (may differ from input)
       ImageInfo GetDimensions() const;
       
       // Return transformed tile
       Tile GetTile(int x, int y, int width, int height) const;
       
       // Propagate or modify ideal tile size
       TileSize GetIdealTileSize() const;
   };

**Ownership:**

Operators own their input by value. This ensures proper lifetime management when 
chaining operations.

SinkBase<Derived>
~~~~~~~~~~~~~~~~~

.. doxygenclass:: fim::SinkBase
   :members:
   :protected-members:

CRTP base class for all sinks. Sinks materialize the pipeline by requesting tiles 
and assembling output.

**Required Methods in Derived Class:**

.. code-block:: cpp

   class MySink : public SinkBase<MySink> {
   public:
       template <typename SourceType>
       void Render(const SourceType& source) {
           // Get source dimensions
           auto dims = source.GetDimensions();
           auto tile_size = source.GetIdealTileSize();
           
           // Process all tiles
           for (int y = 0; y < dims.height; y += tile_size.height) {
               for (int x = 0; x < dims.width; x += tile_size.width) {
                   auto tile = source.GetTile(x, y, tile_size.width, tile_size.height);
                   // Process tile...
               }
           }
       }
   };

ParallelizationMixin<Derived>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenclass:: fim::ParallelizationMixin
   :members:
   :protected-members:

CRTP mixin providing threading utilities for all pipeline components.

**Usage Example:**

.. code-block:: cpp

   class MySource : public SourceBase<MySource> {
       void ProcessTiles() {
           auto &pool = GetThreadPool();
           
           if (ShouldParallelize(num_tiles)) {
               // Parallel processing
               pool.parallelize_loop(0, num_tiles, [&](int i) {
                   ProcessTile(i);
               });
           } else {
               // Sequential processing
               for (int i = 0; i < num_tiles; ++i) {
                   ProcessTile(i);
               }
           }
       }
   };

Sources
-------

TiffSource
~~~~~~~~~~

.. doxygenclass:: fim::TiffSource
   :members:

TIFF image source supporting both tiled and strip-based TIFF files.

**Features:**

- Automatic detection of tiled vs. strip format
- Multi-channel support
- Efficient tile-aligned reads
- Thread-safe tile access

**Example:**

.. code-block:: cpp

   #include <fim/sources/tiff_source.h>
   
   // Create TIFF source
   fim::TiffSource source("input.tiff");
   
   // Get image information
   auto dims = source.GetDimensions();
   auto tile_size = source.GetIdealTileSize();
   
   std::cout << "Image: " << dims.width << "x" << dims.height
             << " channels: " << dims.channels << std::endl;
   std::cout << "Tile size: " << tile_size.width << "x" << tile_size.height << std::endl;
   
   // Read a tile
   auto tile = source.GetTile(0, 0, tile_size.width, tile_size.height);

PngSource
~~~~~~~~~

.. doxygenclass:: fim::PngSource
   :members:

PNG image source using lodepng for decoding.

**Features:**

- RGB and RGBA support
- Automatic color type conversion
- Memory-efficient tile extraction

**Example:**

.. code-block:: cpp

   #include <fim/sources/png_source.h>
   
   // Create PNG source
   fim::PngSource source("input.png");
   
   // Process as tiles
   auto dims = source.GetDimensions();
   auto tile = source.GetTile(0, 0, 256, 256);

MemorySource
~~~~~~~~~~~~

.. doxygenclass:: fim::MemorySource
   :members:

Image source from in-memory buffer.

**Features:**

- Zero-copy when buffer lifetime is managed externally
- Supports arbitrary dimensions and channels
- Useful for NumPy integration

**Example:**

.. code-block:: cpp

   #include <fim/sources/memory_source.h>
   
   // Create from buffer
   std::vector<uint8_t> data(512 * 512 * 3);
   // ... fill data ...
   
   fim::MemorySource source(data, 512, 512, 3);
   
   // Use in pipeline
   fim::Image(std::move(source))
       .Crop(0, 0, 256, 256)
       .Render(fim::LodePngSink("output.png"));

Operators
---------

Crop
~~~~

.. doxygenclass:: fim::Crop
   :members:

Crop operator extracts a rectangular region from the input.

**Example:**

.. code-block:: cpp

   #include <fim/operators/crop.h>
   
   // Crop a region
   fim::CreateTiffImage("input.tiff")
       .Crop(100, 100, 512, 512)  // x, y, width, height
       .Render(fim::LodePngSink("cropped.png"));
   
   // Chain multiple crops
   fim::CreateTiffImage("input.tiff")
       .Crop(0, 0, 1024, 1024)     // First crop
       .Crop(256, 256, 512, 512)   // Second crop (relative to first)
       .Render(fim::LodePngSink("double_cropped.png"));

Downsample
~~~~~~~~~~

.. doxygenclass:: fim::Downsample
   :members:

Downsample operator performs average-pooling downsampling by an integer factor.

**Example:**

.. code-block:: cpp

   #include <fim/operators/downsample.h>
   
   // Downsample by factor of 2
   fim::CreateTiffImage("input.tiff")
       .Downsample(2)  // Output will be half size in each dimension
       .Render(fim::LodePngSink("half_size.png"));
   
   // Create multiple resolutions
   auto image = fim::CreateTiffImage("input.tiff");
   
   std::move(image).Downsample(2).Render(fim::LodePngSink("half.png"));
   std::move(image).Downsample(4).Render(fim::LodePngSink("quarter.png"));
   std::move(image).Downsample(8).Render(fim::LodePngSink("eighth.png"));

Resize
~~~~~~

.. doxygenclass:: fim::Resize
   :members:

Resize operator performs high-quality image resampling to arbitrary target dimensions
using advanced kernels (Lanczos, Magic2021).

**Supported Kernels:**

- `KernelType::kLanczos2`: Lanczos kernel with radius 2 (faster)
- `KernelType::kLanczos3`: Lanczos kernel with radius 3 (default, good quality/speed balance)
- `KernelType::kMagic2021`: Magic Kernel Sharp 2021 with radius 4.5 (highest quality)

**Example:**

.. code-block:: cpp

   #include <fim/operators/resize.h>
   
   auto source = fim::TiffSource::Create("input.tiff");
   
   // Basic resize to 512x512 using default Lanczos3 kernel
   auto resized = fim::Resize(std::move(source), 512, 512);
   fim::MemorySink::Create().Render(resized);
   
   // Resize with Magic2021 kernel for highest quality
   auto high_quality = fim::Resize(
       fim::TiffSource::Create("input.tiff"),
       1024, 1024,
       fim::resize::KernelType::kMagic2021
   );
   
   // Resize with subpixel-accurate box parameter
   fim::resize::Box box(10.5F, 20.5F, 510.5F, 520.5F);
   auto cropped_resized = fim::Resize(
       fim::TiffSource::Create("input.tiff"),
       256, 256,
       fim::resize::KernelType::kLanczos3,
       box  // Precise source region
   );
   
   // Use with Image API
   fim::CreateTiffImage("input.tiff")
       .Crop(100, 100, 1000, 1000)
       .Resize(512, 512, fim::resize::KernelType::kMagic2021)
       .Render(fim::TiffSink::Create("output.tiff"));

**Box Parameter:**

The optional `Box` parameter enables subpixel-accurate source region specification:

.. code-block:: cpp

   // Box specifies (x1, y1, x2, y2) in floating-point pixel coordinates
   fim::resize::Box box{
       0.25F,   // x1: left edge
       0.75F,   // y1: top edge  
       99.25F,  // x2: right edge
       99.75F   // y2: bottom edge
   };
   
   // This is useful for:
   // - Subpixel shifts (like MRXS tile alignment)
   // - Precise region extraction combined with scaling
   // - Avoiding rounding errors in coordinate transformations

Stack
~~~~~

.. doxygenclass:: fim::Stack
   :members:

Stack operator combines multiple images along the channel axis.

**Example:**

.. code-block:: cpp

   #include <fim/operators/stack.h>
   
   // Stack three single-channel images into RGB
   std::vector<fim::Image<fim::PngSource>> channels;
   channels.push_back(fim::CreatePngImage("red.png"));
   channels.push_back(fim::CreatePngImage("green.png"));
   channels.push_back(fim::CreatePngImage("blue.png"));
   
   // Note: Stack requires type-erased or uniform input types
   // See Python API for easier stacking interface

Sinks
-----

TiffSink
~~~~~~~~

.. doxygenclass:: fim::TiffSink
   :members:

TIFF sink writes images to tiled or strip TIFF files.

**Features:**

- Automatic format selection (tiled vs. strip)
- Multi-channel support
- Efficient tile-based writing
- LZW compression support

**Example:**

.. code-block:: cpp

   #include <fim/sinks/tiff_sink.h>
   
   // Write to TIFF file
   fim::CreatePngImage("input.png")
       .Downsample(2)
       .Render(fim::TiffSink("output.tiff"));

LodePngSink
~~~~~~~~~~~

.. doxygenclass:: fim::LodePngSink
   :members:

PNG sink writes images to PNG files using the lodepng library.

**Example:**

.. code-block:: cpp

   #include <fim/sinks/lodepng_png_sink.h>
   
   // Write to PNG file
   fim::CreateTiffImage("input.tiff")
       .Crop(0, 0, 512, 512)
       .Render(fim::LodePngSink("output.png"));

MemorySink
~~~~~~~~~~

.. doxygenclass:: fim::MemorySink
   :members:

Memory sink assembles image tiles into an in-memory buffer.

**Features:**

- Contiguous memory layout
- Useful for NumPy conversion
- Zero-copy data access

**Example:**

.. code-block:: cpp

   #include <fim/sinks/memory_sink.h>
   
   // Render to memory
   auto image = fim::CreateTiffImage("input.tiff")
       .Crop(0, 0, 512, 512);
   
   auto dims = image.GetSource().GetDimensions();
   fim::MemorySink sink(dims.width, dims.height, dims.channels);
   
   image.Render(std::move(sink));
   
   // Access data
   const auto& data = sink.GetDataAs<uint8_t>();
   std::cout << "Data size: " << data.size() << " bytes" << std::endl;

Types
-----

Tile
~~~~

.. doxygenstruct:: fim::Tile
   :members:

Represents a rectangular tile of image data.

ImageInfo
~~~~~~~~~~~~~~~

.. doxygenstruct:: fim::ImageInfo
   :members:

Represents the dimensions of an image.

TileSize
~~~~~~~~

.. doxygenstruct:: fim::TileSize
   :members:

Represents the preferred tile size for processing.

Convenience Functions
---------------------

CreateTiffImage
~~~~~~~~~~~~~~~

.. doxygenfunction:: fim::CreateTiffImage

Creates an Image instance from a TIFF file.

**Example:**

.. code-block:: cpp

   auto image = fim::CreateTiffImage("input.tiff");

CreatePngImage
~~~~~~~~~~~~~~

.. doxygenfunction:: fim::CreatePngImage

Creates an Image instance from a PNG file.

**Example:**

.. code-block:: cpp

   auto image = fim::CreatePngImage("input.png");

Complete Examples
-----------------

Basic Pipeline
~~~~~~~~~~~~~~

.. code-block:: cpp

   #include <fim/image.h>
   
   int main() {
       // Simple pipeline: load, crop, downsample, save
       fim::CreateTiffImage("input.tiff")
           .Crop(10, 10, 512, 512)
           .Downsample(2)
           .Render(fim::LodePngSink("output.png"));
       
       return 0;
   }

Complex Processing
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   #include <fim/image.h>
   #include <iostream>
   
   int main() {
       try {
           // Load image
           auto image = fim::CreateTiffImage("large_image.tiff");
           
           // Get dimensions
           auto dims = image.GetSource().GetDimensions();
           std::cout << "Input: " << dims.width << "x" << dims.height << std::endl;
           
           // Extract multiple patches
           for (int i = 0; i < 4; ++i) {
               int x = (i % 2) * 1024;
               int y = (i / 2) * 1024;
               
               auto patch = fim::CreateTiffImage("large_image.tiff")
                   .Crop(x, y, 1024, 1024)
                   .Downsample(2);
               
               std::string filename = "patch_" + std::to_string(i) + ".png";
               patch.Render(fim::LodePngSink(filename));
           }
           
           std::cout << "Processed 4 patches" << std::endl;
       } catch (const std::exception& e) {
           std::cerr << "Error: " << e.what() << std::endl;
           return 1;
       }
       
       return 0;
   }

Memory Management
~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   #include <fim/image.h>
   #include <memory>
   
   class ImageProcessor {
   public:
       void LoadImage(const std::string& path) {
           // Image manages its own resources via RAII
           image_ = std::make_unique<fim::Image<fim::TiffSource>>(
               fim::TiffSource(path)
           );
       }
       
       void ProcessAndSave(const std::string& output_path) {
           if (!image_) {
               throw std::runtime_error("No image loaded");
           }
           
           // Move semantics prevent copying
           std::move(*image_)
               .Crop(0, 0, 512, 512)
               .Downsample(2)
               .Render(fim::LodePngSink(output_path));
       }
   
   private:
       std::unique_ptr<fim::Image<fim::TiffSource>> image_;
   };

Multi-threading
~~~~~~~~~~~~~~~

.. code-block:: cpp

   #include <fim/image.h>
   #include <fim/sinks/memory_sink.h>
   #include <thread>
   #include <vector>
   
   void ProcessTileInParallel() {
       auto source = fim::TiffSource("large_image.tiff");
       auto dims = source.GetDimensions();
       auto tile_size = source.GetIdealTileSize();
       
       // Process tiles in parallel (thread-safe reads)
       std::vector<std::thread> workers;
       
       for (int y = 0; y < dims.height; y += tile_size.height) {
           for (int x = 0; x < dims.width; x += tile_size.width) {
               workers.emplace_back([&source, x, y, &tile_size]() {
                   // Each thread can safely read different tiles
                   auto tile = source.GetTile(x, y, tile_size.width, tile_size.height);
                   // Process tile...
               });
           }
       }
       
       for (auto& worker : workers) {
           worker.join();
       }
   }


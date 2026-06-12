Adding a New Sink
=================

This guide walks you through creating a new sink for the fimage library.
Sinks materialize the pipeline by assembling tiles into final output.

Overview
--------

A sink is responsible for:

- Requesting tiles from the source/pipeline
- Assembling tiles into the final output
- Writing/storing the result in the desired format

Prerequisites
-------------

- C++20 compiler
- Understanding of CRTP pattern
- Knowledge of the output format you're implementing

Step-by-Step Guide
------------------

Step 1: Create Header File
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a new header in `include/fim/sinks/`:

.. code-block:: bash

   touch include/fim/sinks/jpeg_sink.h

Step 2: Define the Class
~~~~~~~~~~~~~~~~~~~~~~~~~

Start with the basic structure inheriting from `SinkBase`:

.. code-block:: cpp

   // include/fim/sinks/jpeg_sink.h
   #ifndef AIFO_FIMAGE_INCLUDE_FIM_SINKS_JPEG_SINK_H_
   #define AIFO_FIMAGE_INCLUDE_FIM_SINKS_JPEG_SINK_H_
   
   #include <filesystem>
   #include <string>
   
   #include "fim/pipeline.h"
   
   namespace fim {
   
   namespace fs = std::filesystem;
   
   /**
    * @brief Sink that writes images to JPEG files.
    *
    * This sink assembles tiles from the pipeline and writes the
    * result as a JPEG file with configurable quality.
    */
   class JpegSink : public SinkBase<JpegSink> {
   public:
       /**
        * @brief Constructs a JPEG sink.
        *
        * @param filename Output JPEG file path
        * @param quality JPEG quality (1-100, default 90)
        */
       explicit JpegSink(const fs::path& filename, int quality = 90);
       
       /**
        * @brief Renders the pipeline to a JPEG file.
        *
        * This method requests all tiles from the source, assembles them,
        * and writes the result as a JPEG file.
        *
        * @tparam SourceType The type of the pipeline source
        * @param source The pipeline source to render
        */
       template <typename SourceType>
       void Render(const SourceType& source);
   
   private:
       fs::path filename_;
       int quality_;
   };
   
   }  // namespace fim
   
   #endif  // AIFO_FIMAGE_INCLUDE_FIM_SINKS_JPEG_SINK_H_

Step 3: Implement Constructor
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create implementation file `src/sinks/jpeg_sink.cpp`:

.. code-block:: cpp

   // src/sinks/jpeg_sink.cpp
   #include "fim/sinks/jpeg_sink.h"
   
   #include <stdexcept>
   
   namespace fim {
   
   JpegSink::JpegSink(const fs::path& filename, int quality)
       : filename_(filename), quality_(quality) {
       if (quality < 1 || quality > 100) {
           throw std::runtime_error("JPEG quality must be in range [1, 100]");
       }
   }
   
   }  // namespace fim

Step 4: Implement Render() Method
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The core rendering logic assembles tiles and writes output:

.. code-block:: cpp

   template <typename SourceType>
   void JpegSink::Render(const SourceType& source) {
       // Get source dimensions
       auto dims = source.GetDimensions();
       auto tile_size = source.GetIdealTileSize();
       
       // Validate dimensions
       if (dims.width <= 0 || dims.height <= 0) {
           throw std::runtime_error("Invalid image dimensions");
       }
       
       // JPEG only supports 1 or 3 channels (grayscale or RGB)
       if (dims.channels != 1 && dims.channels != 3) {
           throw std::runtime_error("JPEG only supports 1 or 3 channels");
       }
       
       // Allocate buffer for entire image
       std::vector<uint8_t> buffer(dims.width * dims.height * dims.channels);
       
       // Process tiles in parallel if beneficial
       auto& pool = GetThreadPool();
       
       if (ShouldParallelize((dims.height / tile_size.height) * 
                             (dims.width / tile_size.width))) {
           // Parallel tile processing
           std::vector<std::pair<int, int>> tile_coords;
           
           for (int y = 0; y < dims.height; y += tile_size.height) {
               for (int x = 0; x < dims.width; x += tile_size.width) {
                   tile_coords.emplace_back(x, y);
               }
           }
           
           pool.parallelize_loop(0, tile_coords.size(), [&](size_t i) {
               auto [x, y] = tile_coords[i];
               int tw = std::min(tile_size.width, dims.width - x);
               int th = std::min(tile_size.height, dims.height - y);
               
               auto tile = source.GetTile(x, y, tw, th);
               
               // Copy tile data to buffer
               for (int ty = 0; ty < tile.height; ++ty) {
                   int buffer_y = y + ty;
                   int buffer_offset = (buffer_y * dims.width + x) * dims.channels;
                   int tile_offset = (ty * tile.width) * dims.channels;
                   int row_bytes = tile.width * dims.channels;
                   
                   std::memcpy(&buffer[buffer_offset],
                              &tile.data[tile_offset],
                              row_bytes);
               }
           });
       } else {
           // Sequential tile processing
           for (int y = 0; y < dims.height; y += tile_size.height) {
               for (int x = 0; x < dims.width; x += tile_size.width) {
                   int tw = std::min(tile_size.width, dims.width - x);
                   int th = std::min(tile_size.height, dims.height - y);
                   
                   auto tile = source.GetTile(x, y, tw, th);
                   
                   // Copy tile to buffer (same as above)
                   for (int ty = 0; ty < tile.height; ++ty) {
                       int buffer_y = y + ty;
                       int buffer_offset = (buffer_y * dims.width + x) * dims.channels;
                       int tile_offset = (ty * tile.width) * dims.channels;
                       int row_bytes = tile.width * dims.channels;
                       
                       std::memcpy(&buffer[buffer_offset],
                                  &tile.data[tile_offset],
                                  row_bytes);
                   }
               }
           }
       }
       
       // Write JPEG file (using hypothetical JPEG library)
       WriteJpegFile(filename_, buffer, dims.width, dims.height, 
                     dims.channels, quality_);
   }
   
   // Helper function to write JPEG (implementation depends on library)
   void WriteJpegFile(const fs::path& filename,
                      const std::vector<uint8_t>& data,
                      int width, int height, int channels, int quality) {
       // This would use libjpeg, stb_image_write, or another JPEG library
       // Pseudocode:
       // FILE* file = fopen(filename.c_str(), "wb");
       // jpeg_compress(file, data.data(), width, height, channels, quality);
       // fclose(file);
       
       throw std::runtime_error("JPEG writing not yet implemented");
   }

Step 5: Add Tests
~~~~~~~~~~~~~~~~~

Create comprehensive tests in `src/sinks/jpeg_sink_test.cpp`:

.. code-block:: cpp

   #include "fim/sinks/jpeg_sink.h"
   #include "fim/sources/memory_source.h"
   
   #include <gtest/gtest.h>
   
   TEST(JpegSinkTest, Construction) {
       EXPECT_NO_THROW(fim::JpegSink("output.jpg", 90));
       EXPECT_NO_THROW(fim::JpegSink("output.jpg", 1));
       EXPECT_NO_THROW(fim::JpegSink("output.jpg", 100));
   }
   
   TEST(JpegSinkTest, InvalidQuality) {
       EXPECT_THROW(fim::JpegSink("output.jpg", 0), std::runtime_error);
       EXPECT_THROW(fim::JpegSink("output.jpg", 101), std::runtime_error);
       EXPECT_THROW(fim::JpegSink("output.jpg", -1), std::runtime_error);
   }
   
   TEST(JpegSinkTest, RenderSmallImage) {
       std::vector<uint8_t> data(100 * 100 * 3, 128);  // Gray image
       fim::MemorySource source(data, 100, 100, 3);
       
       fim::JpegSink sink("test_output.jpg", 90);
       
       // Note: This will throw until WriteJpegFile is implemented
       // EXPECT_NO_THROW(sink.Render(source));
   }
   
   TEST(JpegSinkTest, UnsupportedChannels) {
       std::vector<uint8_t> data(100 * 100 * 4);  // RGBA
       fim::MemorySource source(data, 100, 100, 4);
       
       fim::JpegSink sink("output.jpg");
       
       // Should throw for 4-channel image
       EXPECT_THROW(sink.Render(source), std::runtime_error);
   }

Step 6: Update BUILD.bazel
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add sink to build configuration:

.. code-block:: python

   cc_library(
       name = "fimage",
       srcs = glob([
           "src/**/*.cpp",
           "src/sinks/jpeg_sink.cpp",  # Add your sink
       ], exclude=["src/**/*_test.cpp"]),
       hdrs = glob(["include/**/*.h"]),
       # Add JPEG library dependency if needed
       deps = [
           # ... existing deps
           # "@libjpeg//:jpeg",  # If using libjpeg
       ],
   )
   
   cc_test(
       name = "jpeg_sink_test",
       srcs = ["src/sinks/jpeg_sink_test.cpp"],
       deps = [
           ":fimage",
           "@googletest//:gtest",
           "@googletest//:gtest_main",
       ],
   )

Step 7: Build and Test
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   bazelisk build //aifo/fimage:fimage
   bazelisk test //aifo/fimage:jpeg_sink_test

Step 8: Use Your Sink
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   #include <fim/image.h>
   #include <fim/sinks/jpeg_sink.h>
   
   int main() {
       // Process and save as JPEG
       fim::CreateTiffImage("input.tiff")
           .Crop(0, 0, 512, 512)
           .Downsample(2)
           .Render(fim::JpegSink("output.jpg", 85));  // Quality 85
       
       return 0;
   }

Advanced Topics
---------------

Streaming Sinks
~~~~~~~~~~~~~~~

For very large images, write tiles directly without buffering:

.. code-block:: cpp

   template <typename SourceType>
   void StreamingSink::Render(const SourceType& source) {
       auto dims = source.GetDimensions();
       auto tile_size = source.GetIdealTileSize();
       
       // Open output file
       auto file = OpenOutputFile(filename_, dims);
       
       // Write tiles directly (must be in order for some formats)
       for (int y = 0; y < dims.height; y += tile_size.height) {
           for (int x = 0; x < dims.width; x += tile_size.width) {
               auto tile = source.GetTile(x, y, tile_size.width, tile_size.height);
               WriteTileToFile(file, tile, x, y);
           }
       }
       
       CloseOutputFile(file);
   }

Move Semantics for Resource Transfer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Support move construction for efficient resource handling:

.. code-block:: cpp

   class BufferedSink : public SinkBase<BufferedSink> {
   public:
       explicit BufferedSink(size_t buffer_size)
           : buffer_(buffer_size) {}
       
       // Move constructor
       BufferedSink(BufferedSink&& other) noexcept
           : buffer_(std::move(other.buffer_)) {}
       
       // Move assignment
       BufferedSink& operator=(BufferedSink&& other) noexcept {
           buffer_ = std::move(other.buffer_);
           return *this;
       }
       
       // Deleted copy
       BufferedSink(const BufferedSink&) = delete;
       BufferedSink& operator=(const BufferedSink&) = delete;
   
   private:
       std::vector<uint8_t> buffer_;
   };

Sinks with Metadata
~~~~~~~~~~~~~~~~~~~

Some formats require metadata alongside pixel data:

.. code-block:: cpp

   class MetadataSink : public SinkBase<MetadataSink> {
   public:
       void SetMetadata(const std::string& key, const std::string& value) {
           metadata_[key] = value;
       }
       
       template <typename SourceType>
       void Render(const SourceType& source) {
           auto dims = source.GetDimensions();
           
           // Assemble image...
           
           // Write with metadata
           WriteWithMetadata(filename_, buffer, dims, metadata_);
       }
   
   private:
       std::map<std::string, std::string> metadata_;
   };

Best Practices
--------------

1. **Handle format constraints**: Validate channel count, dimensions
2. **Efficient memory usage**: Stream when possible, buffer when needed
3. **Parallel tile processing**: Use thread pool for large images
4. **Error handling**: Check file I/O, validate inputs
5. **Resource management**: Use RAII for file handles, buffers
6. **Move semantics**: Support efficient resource transfer
7. **Progress reporting**: For long operations, consider callbacks
8. **Metadata preservation**: Carry through relevant metadata

Common Pitfalls
---------------

**Not handling partial tiles at boundaries:**

.. code-block:: cpp

   // Bad: Assumes all tiles are full size
   for (int y = 0; y < dims.height; y += tile_size.height) {
       for (int x = 0; x < dims.width; x += tile_size.width) {
           auto tile = source.GetTile(x, y, tile_size.width, tile_size.height);
           // tile might be smaller at boundaries!
           std::memcpy(&buffer[offset], tile.data.data(), 
                      tile_size.width * tile_size.height * channels);  // Wrong!
       }
   }
   
   // Good: Use actual tile dimensions
   for (int y = 0; y < dims.height; y += tile_size.height) {
       for (int x = 0; x < dims.width; x += tile_size.width) {
           auto tile = source.GetTile(x, y, tile_size.width, tile_size.height);
           
           // Use tile.width and tile.height, not tile_size
           for (int ty = 0; ty < tile.height; ++ty) {
               int row_bytes = tile.width * channels;
               std::memcpy(&buffer[...], &tile.data[ty * row_bytes], row_bytes);
           }
       }
   }

**Memory leaks in file I/O:**

.. code-block:: cpp

   // Bad: Manual file management
   template <typename SourceType>
   void Render(const SourceType& source) {
       FILE* file = fopen(filename_.c_str(), "wb");
       // ... processing ...
       // If exception thrown, file never closed!
       fclose(file);
   }
   
   // Good: RAII file handle
   template <typename SourceType>
   void Render(const SourceType& source) {
       auto file = std::unique_ptr<FILE, decltype(&fclose)>(
           fopen(filename_.c_str(), "wb"),
           &fclose
       );
       
       if (!file) {
           throw std::runtime_error("Cannot open file");
       }
       
       // ... processing ...
       // File automatically closed even if exception thrown
   }

**Race conditions in parallel writes:**

.. code-block:: cpp

   // Bad: Concurrent writes to same buffer region
   pool.parallelize_loop(0, num_tiles, [&](size_t i) {
       auto tile = source.GetTile(...);
       
       // Multiple threads might write to overlapping regions!
       std::memcpy(&shared_buffer[offset], ...);  // Race condition
   });
   
   // Good: Ensure non-overlapping writes
   pool.parallelize_loop(0, num_tiles, [&](size_t i) {
       auto [x, y] = tile_coords[i];
       auto tile = source.GetTile(x, y, tw, th);
       
       // Each tile writes to its own region (no overlap)
       int offset = (y * width + x) * channels;
       std::memcpy(&shared_buffer[offset], ...);  // Safe
   });

Next Steps
----------

- See :doc:`python_bindings` to expose your sink to Python
- Reference existing sinks in `include/fim/sinks/`
- Consider contributing your sink to fimage!


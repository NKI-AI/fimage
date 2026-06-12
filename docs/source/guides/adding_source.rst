Adding a New Source
===================

This guide walks you through creating a new image source for the fimage library.
Sources provide image data from files, network, databases, or any other origin.

Overview
--------

A source is responsible for:

- Providing image dimensions (width, height, channels)
- Delivering image data as tiles
- Suggesting optimal tile sizes for efficiency

Prerequisites
-------------

- C++20 compiler
- Understanding of CRTP pattern
- Knowledge of the image format you're implementing

Step-by-Step Guide
------------------

Step 1: Create Header File
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a new header file in `include/fim/sources/`:

.. code-block:: bash

   touch include/fim/sources/gradient_source.h

Step 2: Define the Class
~~~~~~~~~~~~~~~~~~~~~~~~~

Start with the basic structure inheriting from `SourceBase`:

.. code-block:: cpp

   // include/fim/sources/gradient_source.h
   #ifndef AIFO_FIMAGE_INCLUDE_FIM_SOURCES_GRADIENT_SOURCE_H_
   #define AIFO_FIMAGE_INCLUDE_FIM_SOURCES_GRADIENT_SOURCE_H_
   
   #include "fim/pipeline.h"
   #include "fim/types.h"
   
   namespace fim {
   
   /**
    * @brief Source that generates a gradient image procedurally.
    *
    * This source creates a horizontal gradient from black to white
    * without requiring any input file. Useful for testing and demos.
    */
   class GradientSource : public SourceBase<GradientSource> {
   public:
       /**
        * @brief Constructs a gradient source with specified dimensions.
        *
        * @param width Width of the generated image
        * @param height Height of the generated image
        * @param channels Number of channels (gradient repeated per channel)
        */
       GradientSource(int width, int height, int channels = 3);
       
       // Required interface methods
       ImageInfo GetDimensions() const;
       Tile GetTile(int x, int y, int width, int height) const;
       TileSize GetIdealTileSize() const;
   
   private:
       int width_;
       int height_;
       int channels_;
   };
   
   }  // namespace fim
   
   #endif  // AIFO_FIMAGE_INCLUDE_FIM_SOURCES_GRADIENT_SOURCE_H_

Step 3: Implement GetDimensions()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This method returns the image dimensions:

.. code-block:: cpp

   ImageInfo GradientSource::GetDimensions() const {
       return ImageInfo(width_, height_, channels_);
   }

Step 4: Implement GetIdealTileSize()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Suggest an optimal tile size. For procedural sources, 256x256 is a good default:

.. code-block:: cpp

   TileSize GradientSource::GetIdealTileSize() const {
       return TileSize(256, 256);
   }

Step 5: Implement GetTile()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is the core method that generates/retrieves tile data:

.. code-block:: cpp

   Tile GradientSource::GetTile(int x, int y, int width, int height) const {
       // Clamp requested region to image bounds
       int actual_width = std::min(width, width_ - x);
       int actual_height = std::min(height, height_ - y);
       
       if (actual_width <= 0 || actual_height <= 0) {
           return Tile(x, y, 0, 0, channels_);
       }
       
       // Create tile with proper dimensions
       Tile tile(x, y, actual_width, actual_height, channels_);
       
       // Generate gradient data
       for (int ty = 0; ty < actual_height; ++ty) {
           for (int tx = 0; tx < actual_width; ++tx) {
               // Calculate global X position for gradient
               int global_x = x + tx;
               
               // Gradient value: 0 (black) at left, 255 (white) at right
               uint8_t value = static_cast<uint8_t>(
                   (global_x * 255) / (width_ - 1)
               );
               
               // Set value for all channels
               int pixel_offset = (ty * actual_width + tx) * channels_;
               for (int c = 0; c < channels_; ++c) {
                   tile.data[pixel_offset + c] = value;
               }
           }
       }
       
       return tile;
   }

Step 6: Implement Constructor
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Complete implementation file `src/sources/gradient_source.cpp`:

.. code-block:: cpp

   // src/sources/gradient_source.cpp
   #include "fim/sources/gradient_source.h"
   
   #include <stdexcept>
   
   namespace fim {
   
   GradientSource::GradientSource(int width, int height, int channels)
       : width_(width), height_(height), channels_(channels) {
       if (width <= 0 || height <= 0) {
           throw std::runtime_error("Invalid dimensions for gradient source");
       }
       if (channels <= 0 || channels > 4) {
           throw std::runtime_error("Invalid number of channels");
       }
   }
   
   ImageInfo GradientSource::GetDimensions() const {
       return ImageInfo(width_, height_, channels_);
   }
   
   Tile GradientSource::GetTile(int x, int y, int width, int height) const {
       // Implementation from Step 5
       // ...
   }
   
   TileSize GradientSource::GetIdealTileSize() const {
       return TileSize(256, 256);
   }
   
   }  // namespace fim

Step 7: Add Tests
~~~~~~~~~~~~~~~~~

Create comprehensive tests in `src/sources/gradient_source_test.cpp`:

.. code-block:: cpp

   #include "fim/sources/gradient_source.h"
   
   #include <gtest/gtest.h>
   
   TEST(GradientSourceTest, BasicCreation) {
       fim::GradientSource source(512, 512, 3);
       
       auto dims = source.GetDimensions();
       EXPECT_EQ(512, dims.width);
       EXPECT_EQ(512, dims.height);
       EXPECT_EQ(3, dims.channels);
   }
   
   TEST(GradientSourceTest, TileRetrieval) {
       fim::GradientSource source(512, 512, 3);
       
       auto tile = source.GetTile(0, 0, 256, 256);
       EXPECT_EQ(0, tile.x);
       EXPECT_EQ(0, tile.y);
       EXPECT_EQ(256, tile.width);
       EXPECT_EQ(256, tile.height);
       EXPECT_EQ(3, tile.channels);
       EXPECT_EQ(256 * 256 * 3, tile.data.size());
   }
   
   TEST(GradientSourceTest, GradientValues) {
       fim::GradientSource source(256, 256, 1);
       
       // Check left edge (should be 0/black)
       auto left_tile = source.GetTile(0, 0, 1, 1);
       EXPECT_EQ(0, left_tile.data[0]);
       
       // Check right edge (should be 255/white)
       auto right_tile = source.GetTile(255, 0, 1, 1);
       EXPECT_EQ(255, right_tile.data[0]);
       
       // Check middle (should be ~127)
       auto mid_tile = source.GetTile(128, 0, 1, 1);
       EXPECT_NEAR(127, mid_tile.data[0], 2);
   }
   
   TEST(GradientSourceTest, BoundsClamping) {
       fim::GradientSource source(100, 100, 3);
       
       // Request tile beyond bounds
       auto tile = source.GetTile(90, 90, 20, 20);
       EXPECT_EQ(10, tile.width);   // Clamped to remaining width
       EXPECT_EQ(10, tile.height);  // Clamped to remaining height
   }
   
   TEST(GradientSourceTest, InvalidDimensions) {
       EXPECT_THROW(fim::GradientSource(0, 100, 3), std::runtime_error);
       EXPECT_THROW(fim::GradientSource(100, 0, 3), std::runtime_error);
       EXPECT_THROW(fim::GradientSource(100, 100, 0), std::runtime_error);
   }

Step 8: Update BUILD.bazel
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add your source to the build configuration:

.. code-block:: python

   # aifo/fimage/BUILD.bazel
   
   cc_library(
       name = "fimage",
       srcs = glob([
           "src/**/*.cpp",
           "src/sources/gradient_source.cpp",  # Add your source
       ], exclude=["src/**/*_test.cpp"]),
       hdrs = glob([
           "include/**/*.h",
       ]),
       # ... rest of configuration
   )
   
   cc_test(
       name = "gradient_source_test",
       srcs = ["src/sources/gradient_source_test.cpp"],
       deps = [
           ":fimage",
           "@googletest//:gtest",
           "@googletest//:gtest_main",
       ],
   )

Step 9: Build and Test
~~~~~~~~~~~~~~~~~~~~~~

Build and run tests:

.. code-block:: bash

   # Build the library
   bazelisk build //aifo/fimage:fimage
   
   # Run tests
   bazelisk test //aifo/fimage:gradient_source_test
   
   # Run all tests
   bazelisk test //aifo/fimage/...

Step 10: Use Your Source
~~~~~~~~~~~~~~~~~~~~~~~~~

Now you can use your new source in pipelines:

.. code-block:: cpp

   #include <fim/image.h>
   #include <fim/sources/gradient_source.h>
   #include <fim/sinks/lodepng_png_sink.h>
   
   int main() {
       // Create gradient image
       fim::Image<fim::GradientSource> image(
           fim::GradientSource(1024, 768, 3)
       );
       
       // Process and save
       std::move(image)
           .Crop(256, 192, 512, 384)
           .Downsample(2)
           .Render(fim::LodePngSink("gradient.png"));
       
       return 0;
   }

Advanced Topics
---------------

File-Based Sources
~~~~~~~~~~~~~~~~~~

For sources that read from files, you need to manage file handles:

.. code-block:: cpp

   class MyFileSource : public SourceBase<MyFileSource> {
   public:
       explicit MyFileSource(const fs::path& filename) {
           file_ = fopen(filename.c_str(), "rb");
           if (!file_) {
               throw std::runtime_error("Cannot open file");
           }
           // Read header, initialize dimensions...
       }
       
       ~MyFileSource() {
           if (file_) {
               fclose(file_);
           }
       }
       
       // Disable copying (file handle cannot be shared)
       MyFileSource(const MyFileSource&) = delete;
       MyFileSource& operator=(const MyFileSource&) = delete;
       
       // Enable moving
       MyFileSource(MyFileSource&& other) noexcept
           : file_(other.file_) {
           other.file_ = nullptr;
       }
   
   private:
       FILE* file_ = nullptr;
       int width_;
       int height_;
       int channels_;
   };

Thread Safety
~~~~~~~~~~~~~

Sources must be thread-safe for concurrent tile reads. Use mutexes for non-thread-safe operations:

.. code-block:: cpp

   class ThreadSafeSource : public SourceBase<ThreadSafeSource> {
   public:
       Tile GetTile(int x, int y, int width, int height) const {
           std::lock_guard<std::mutex> lock(mutex_);
           
           // Thread-safe file operations
           fseek(file_, offset, SEEK_SET);
           // ... read data ...
           
           return tile;
       }
   
   private:
       mutable std::mutex mutex_;  // mutable for const methods
       FILE* file_;
   };

Efficient Tile Access
~~~~~~~~~~~~~~~~~~~~~

Optimize tile access to match the underlying format:

.. code-block:: cpp

   TileSize GetIdealTileSize() const {
       // For tiled TIFFs, return the native tile size
       if (is_tiled_) {
           return TileSize(tile_width_, tile_height_);
       }
       
       // For strip-based formats, return full-width strips
       return TileSize(width_, strip_height_);
   }

Resource Management
~~~~~~~~~~~~~~~~~~~

Use RAII for all resources:

.. code-block:: cpp

   class RAIISource : public SourceBase<RAIISource> {
   public:
       explicit RAIISource(const fs::path& filename) 
           : file_(fopen(filename.c_str(), "rb"), &fclose),
             buffer_(new uint8_t[BUFFER_SIZE]) {
           if (!file_) {
               throw std::runtime_error("Cannot open file");
           }
       }
   
   private:
       std::unique_ptr<FILE, decltype(&fclose)> file_;
       std::unique_ptr<uint8_t[]> buffer_;
   };

Best Practices
--------------

1. **Validate inputs**: Check dimensions, file existence, format validity
2. **Handle errors gracefully**: Throw meaningful exceptions
3. **Optimize for common cases**: Use efficient tile sizes
4. **Document thoroughly**: Add Doxygen comments
5. **Test extensively**: Cover edge cases, error conditions
6. **Thread-safe by default**: Protect shared state with mutexes
7. **Use RAII**: Manage resources automatically
8. **Clamp tile requests**: Handle out-of-bounds gracefully

Common Pitfalls
---------------

**Forgetting to clamp tile coordinates:**

.. code-block:: cpp

   // Bad: Can read out of bounds
   Tile GetTile(int x, int y, int width, int height) const {
       Tile tile(x, y, width, height, channels_);
       // ... fill tile.data ...
       return tile;
   }
   
   // Good: Clamp to valid region
   Tile GetTile(int x, int y, int width, int height) const {
       int actual_width = std::min(width, width_ - x);
       int actual_height = std::min(height, height_ - y);
       Tile tile(x, y, actual_width, actual_height, channels_);
       // ... fill tile.data ...
       return tile;
   }

**Not being thread-safe:**

.. code-block:: cpp

   // Bad: Not thread-safe
   Tile GetTile(int x, int y, int width, int height) const {
       fseek(file_, offset, SEEK_SET);  // Race condition!
       // ...
   }
   
   // Good: Protected by mutex
   Tile GetTile(int x, int y, int width, int height) const {
       std::lock_guard<std::mutex> lock(mutex_);
       fseek(file_, offset, SEEK_SET);
       // ...
   }

**Memory leaks:**

.. code-block:: cpp

   // Bad: Manual memory management
   MySource() {
       buffer_ = new uint8_t[SIZE];
   }
   ~MySource() {
       delete[] buffer_;  // Easy to forget
   }
   
   // Good: RAII with smart pointers
   MySource() : buffer_(new uint8_t[SIZE]) {}
   
   private:
       std::unique_ptr<uint8_t[]> buffer_;  // Automatic cleanup

Next Steps
----------

- See :doc:`adding_operator` to add transformations
- See :doc:`python_bindings` to expose your source to Python
- Reference existing sources in `include/fim/sources/`


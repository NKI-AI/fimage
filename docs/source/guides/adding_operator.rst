Adding a New Operator
=====================

This guide walks you through creating a new image operator for the fimage library.
Operators transform image data in the processing pipeline.

Overview
--------

An operator is responsible for:

- Transforming image dimensions (if applicable)
- Transforming tile data from input to output
- Propagating or modifying ideal tile sizes

Prerequisites
-------------

- C++20 compiler
- Understanding of CRTP pattern and template metaprogramming
- Knowledge of the transformation you're implementing

Step-by-Step Guide
------------------

Step 1: Create Header File
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a new header in `include/fim/operators/`:

.. code-block:: bash

   touch include/fim/operators/rotate90.h

Step 2: Define the Class Template
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Operators are templates that take the input type as a parameter:

.. code-block:: cpp

   // include/fim/operators/rotate90.h
   #ifndef AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_ROTATE90_H_
   #define AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_ROTATE90_H_
   
   #include <utility>
   
   #include "fim/pipeline.h"
   #include "fim/types.h"
   
   namespace fim {
   
   /**
    * @brief Operator that rotates an image 90 degrees clockwise.
    *
    * This operator rotates the input image 90 degrees clockwise,
    * swapping width and height and transforming tile coordinates
    * accordingly.
    *
    * @tparam InputType The type of the input source or operator
    */
   template <typename InputType>
   class Rotate90 : public OperatorBase<Rotate90<InputType>, InputType> {
   public:
       /**
        * @brief Constructs a rotate operator.
        *
        * Takes ownership of the input to ensure proper lifetime
        * management when chaining operations.
        *
        * @param input The input source or operator to rotate
        */
       explicit Rotate90(InputType input)
           : OperatorBase<Rotate90<InputType>, InputType>(std::move(input)) {}
       
       // Required interface methods
       ImageInfo GetDimensions() const;
       Tile GetTile(int x, int y, int width, int height) const;
       TileSize GetIdealTileSize() const;
   };
   
   }  // namespace fim
   
   #endif  // AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_ROTATE90_H_

Step 3: Implement GetDimensions()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Transform input dimensions to output dimensions:

.. code-block:: cpp

   template <typename InputType>
   ImageInfo Rotate90<InputType>::GetDimensions() const {
       auto input_dims = this->input_.GetDimensions();
       
       // Rotation swaps width and height
       return ImageInfo(
           input_dims.height,   // Output width = input height
           input_dims.width,    // Output height = input width
           input_dims.channels  // Channels unchanged
       );
   }

Step 4: Implement GetIdealTileSize()
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Propagate or transform the ideal tile size:

.. code-block:: cpp

   template <typename InputType>
   TileSize Rotate90<InputType>::GetIdealTileSize() const {
       auto input_tile_size = this->input_.GetIdealTileSize();
       
       // Rotation swaps tile dimensions
       return TileSize(input_tile_size.height, input_tile_size.width);
   }

Step 5: Implement GetTile()
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is the core transformation logic:

.. code-block:: cpp

   template <typename InputType>
   Tile Rotate90<InputType>::GetTile(int x, int y, int width, int height) const {
       auto output_dims = GetDimensions();
       auto input_dims = this->input_.GetDimensions();
       
       // Clamp to output bounds
       int actual_width = std::min(width, output_dims.width - x);
       int actual_height = std::min(height, output_dims.height - y);
       
       if (actual_width <= 0 || actual_height <= 0) {
           return Tile(x, y, 0, 0, input_dims.channels);
       }
       
       // For 90-degree clockwise rotation:
       // output(x, y) = input(input.height - 1 - y, x)
       //
       // Output tile at (x, y, width, height) corresponds to
       // input region that we need to sample from
       
       // Request input tile (note: may need larger region)
       // For simplicity, we request a region that covers needed pixels
       int input_x = input_dims.height - 1 - (y + actual_height - 1);
       int input_y = x;
       int input_width = actual_height;
       int input_height = actual_width;
       
       auto input_tile = this->input_.GetTile(
           input_x, input_y, input_width, input_height
       );
       
       // Create output tile
       Tile output_tile(x, y, actual_width, actual_height, input_dims.channels);
       
       // Perform rotation transformation
       for (int out_y = 0; out_y < actual_height; ++out_y) {
           for (int out_x = 0; out_x < actual_width; ++out_x) {
               // Calculate source position in input tile
               int in_y = out_x;
               int in_x = actual_height - 1 - out_y;
               
               // Copy pixel data for all channels
               int out_offset = (out_y * actual_width + out_x) * input_dims.channels;
               int in_offset = (in_y * input_tile.width + in_x) * input_dims.channels;
               
               for (int c = 0; c < input_dims.channels; ++c) {
                   output_tile.data[out_offset + c] = input_tile.data[in_offset + c];
               }
           }
       }
       
       return output_tile;
   }

Step 6: Add to Image Class (Optional)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For convenience, add a method to the Image class in `include/fim/image.h`:

.. code-block:: cpp

   // In Image class definition
   /**
    * @brief Applies a 90-degree clockwise rotation.
    *
    * @return New Image instance with rotation applied
    */
   auto Rotate90() && {
       return Image<fim::Rotate90<SourceType>>(
           fim::Rotate90<SourceType>(std::move(source_))
       );
   }

Step 7: Create Tests
~~~~~~~~~~~~~~~~~~~~

Create comprehensive tests in `src/operators/rotate90_test.cpp`:

.. code-block:: cpp

   #include "fim/operators/rotate90.h"
   #include "fim/sources/memory_source.h"
   
   #include <gtest/gtest.h>
   
   TEST(Rotate90Test, DimensionTransform) {
       // Create 100x200 source
       std::vector<uint8_t> data(100 * 200 * 3);
       fim::MemorySource source(data, 100, 200, 3);
       
       // Apply rotation
       fim::Rotate90 rotated(std::move(source));
       
       // Should become 200x100
       auto dims = rotated.GetDimensions();
       EXPECT_EQ(200, dims.width);   // Was height
       EXPECT_EQ(100, dims.height);  // Was width
       EXPECT_EQ(3, dims.channels);  // Unchanged
   }
   
   TEST(Rotate90Test, TileSizeTransform) {
       std::vector<uint8_t> data(100 * 200 * 3);
       fim::MemorySource source(data, 100, 200, 3);
       fim::Rotate90 rotated(std::move(source));
       
       auto tile_size = rotated.GetIdealTileSize();
       // Memory source ideal tile size is typically 256x256,
       // rotation should swap dimensions
       EXPECT_GT(tile_size.width, 0);
       EXPECT_GT(tile_size.height, 0);
   }
   
   TEST(Rotate90Test, PixelTransformation) {
       // Create 2x2 image with known pattern
       std::vector<uint8_t> data = {
           // Row 0: [red=100, green=200]
           100, 0, 0,   200, 0, 0,
           // Row 1: [blue=150, yellow=250]
           0, 0, 150,   250, 250, 0
       };
       
       fim::MemorySource source(data, 2, 2, 3);
       fim::Rotate90 rotated(std::move(source));
       
       // After 90° clockwise rotation:
       // Original:     Rotated:
       // [100, 200]    [150, 100]
       // [150, 250]    [250, 200]
       
       auto tile = rotated.GetTile(0, 0, 2, 2);
       
       // Check rotated pixels
       // Top-left should be blue (0, 0, 150)
       EXPECT_EQ(0, tile.data[0]);
       EXPECT_EQ(0, tile.data[1]);
       EXPECT_EQ(150, tile.data[2]);
       
       // Top-right should be red (100, 0, 0)
       EXPECT_EQ(100, tile.data[3]);
       EXPECT_EQ(0, tile.data[4]);
       EXPECT_EQ(0, tile.data[5]);
   }
   
   TEST(Rotate90Test, PartialTiles) {
       std::vector<uint8_t> data(100 * 200 * 3);
       fim::MemorySource source(data, 100, 200, 3);
       fim::Rotate90 rotated(std::move(source));
       
       // Request partial tile
       auto tile = rotated.GetTile(0, 0, 50, 50);
       EXPECT_EQ(50, tile.width);
       EXPECT_EQ(50, tile.height);
   }
   
   TEST(Rotate90Test, BoundsClam ping) {
       std::vector<uint8_t> data(100 * 200 * 3);
       fim::MemorySource source(data, 100, 200, 3);
       fim::Rotate90 rotated(std::move(source));
       
       auto dims = rotated.GetDimensions();
       
       // Request beyond bounds
       auto tile = rotated.GetTile(dims.width - 10, dims.height - 10, 20, 20);
       EXPECT_EQ(10, tile.width);
       EXPECT_EQ(10, tile.height);
   }

Step 8: Update BUILD.bazel
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Since operators are header-only templates, you typically just need to add tests:

.. code-block:: python

   cc_test(
       name = "rotate90_test",
       srcs = ["src/operators/rotate90_test.cpp"],
       deps = [
           ":fimage",
           "@googletest//:gtest",
           "@googletest//:gtest_main",
       ],
   )

Step 9: Build and Test
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   bazelisk test //aifo/fimage:rotate90_test
   bazelisk test //aifo/fimage/...

Step 10: Use Your Operator
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   #include <fim/image.h>
   #include <fim/operators/rotate90.h>
   
   int main() {
       // Load and rotate image
       fim::CreateTiffImage("input.tiff")
           .Rotate90()  // If added to Image class
           .Render(fim::LodePngSink("rotated.png"));
       
       // Or use directly
       auto source = fim::TiffSource("input.tiff");
       auto rotated = fim::Rotate90(std::move(source));
       fim::Image(std::move(rotated))
           .Render(fim::LodePngSink("rotated.png"));
       
       return 0;
   }

Advanced Topics
---------------

Multi-Pass Operators
~~~~~~~~~~~~~~~~~~~~

Some operations require multiple passes over the input:

.. code-block:: cpp

   template <typename InputType>
   class Normalize : public OperatorBase<Normalize<InputType>, InputType> {
   public:
       explicit Normalize(InputType input)
           : OperatorBase<Normalize, InputType>(std::move(input)) {
           // First pass: compute min/max
           ComputeMinMax();
       }
   
   private:
       void ComputeMinMax() {
           auto dims = this->input_.GetDimensions();
           auto tile_size = this->input_.GetIdealTileSize();
           
           uint8_t min_val = 255;
           uint8_t max_val = 0;
           
           // Scan entire image
           for (int y = 0; y < dims.height; y += tile_size.height) {
               for (int x = 0; x < dims.width; x += tile_size.width) {
                   auto tile = this->input_.GetTile(x, y, tile_size.width, tile_size.height);
                   for (uint8_t val : tile.data) {
                       min_val = std::min(min_val, val);
                       max_val = std::max(max_val, val);
                   }
               }
           }
           
           min_ = min_val;
           max_ = max_val;
       }
       
       uint8_t min_;
       uint8_t max_;
   };

Stateful Operators
~~~~~~~~~~~~~~~~~~

Operators can maintain state for optimization:

.. code-block:: cpp

   template <typename InputType>
   class CachingOperator : public OperatorBase<CachingOperator<InputType>, InputType> {
   public:
       Tile GetTile(int x, int y, int width, int height) const {
           // Check cache
           TileKey key{x, y, width, height};
           auto it = cache_.find(key);
           if (it != cache_.end()) {
               return it->second;
           }
           
           // Compute and cache
           auto tile = ComputeTile(x, y, width, height);
           cache_[key] = tile;
           return tile;
       }
   
   private:
       struct TileKey {
           int x, y, width, height;
           bool operator<(const TileKey& other) const {
               return std::tie(x, y, width, height) < 
                      std::tie(other.x, other.y, other.width, other.height);
           }
       };
       
       mutable std::map<TileKey, Tile> cache_;
   };

Operators with Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~

Pass configuration parameters to operators:

.. code-block:: cpp

   template <typename InputType>
   class Brightness : public OperatorBase<Brightness<InputType>, InputType> {
   public:
       Brightness(InputType input, float factor)
           : OperatorBase<Brightness, InputType>(std::move(input)),
             factor_(factor) {
           if (factor_ < 0.0f || factor_ > 2.0f) {
               throw std::runtime_error("Brightness factor must be in [0, 2]");
           }
       }
       
       Tile GetTile(int x, int y, int width, int height) const {
           auto input_tile = this->input_.GetTile(x, y, width, height);
           
           // Adjust brightness
           for (auto& pixel : input_tile.data) {
               float adjusted = pixel * factor_;
               pixel = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, adjusted)));
           }
           
           return input_tile;
       }
   
   private:
       float factor_;
   };

Best Practices
--------------

1. **Minimize intermediate allocations**: Reuse buffers when possible
2. **Handle edge cases**: Empty tiles, boundary conditions
3. **Optimize for common cases**: Full-size tiles, aligned access
4. **Document transformations**: Clearly explain coordinate mappings
5. **Test thoroughly**: Verify correctness on various inputs
6. **Consider parallelization**: Can tiles be processed independently?
7. **Validate parameters**: Check inputs in constructor
8. **Preserve type safety**: Use strong typing for parameters

Common Pitfalls
---------------

**Incorrect coordinate transformation:**

.. code-block:: cpp

   // Bad: Forgetting to transform coordinates
   Tile GetTile(int x, int y, int width, int height) const {
       return this->input_.GetTile(x, y, width, height);  // Wrong!
   }
   
   // Good: Proper coordinate transformation
   Tile GetTile(int x, int y, int width, int height) const {
       int input_x = TransformX(x);
       int input_y = TransformY(y);
       return this->input_.GetTile(input_x, input_y, width, height);
   }

**Not owning input by value:**

.. code-block:: cpp

   // Bad: Storing reference (before architecture improvements)
   template <typename InputType>
   class BadOperator : public OperatorBase<BadOperator<InputType>, InputType> {
   public:
       BadOperator(const InputType& input)  // Reference!
           : OperatorBase(input) {}  // Dangling reference in chains
   };
   
   // Good: Taking ownership by value
   template <typename InputType>
   class GoodOperator : public OperatorBase<GoodOperator<InputType>, InputType> {
   public:
       explicit GoodOperator(InputType input)  // By value
           : OperatorBase(std::move(input)) {}  // Move to base
   };

**Inefficient tile requests:**

.. code-block:: cpp

   // Bad: Requesting many small tiles
   Tile GetTile(int x, int y, int width, int height) const {
       for (int ty = y; ty < y + height; ++ty) {
           for (int tx = x; tx < x + width; ++tx) {
               auto pixel_tile = this->input_.GetTile(tx, ty, 1, 1);  // Very slow!
           }
       }
   }
   
   // Good: Request entire region at once
   Tile GetTile(int x, int y, int width, int height) const {
       return this->input_.GetTile(x, y, width, height);
   }

Next Steps
----------

- See :doc:`adding_sink` to create output formats
- See :doc:`python_bindings` to expose your operator to Python
- Reference existing operators in `include/fim/operators/`


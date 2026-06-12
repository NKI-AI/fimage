// Copyright 2025 Jonas Teuwen. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fim/sources/memory_source.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "fim/types.h"

namespace fim {

class MemorySourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test data for various test scenarios
    CreateTestData();
  }

  void TearDown() override {
    // No cleanup needed for memory-based tests
  }

 private:
  void CreateTestData() {
    // RGB test image 10x8x3
    rgb_data_ = CreateImageData(10, 8, 3);
    rgb_dims_ =
        ImageInfo(10, 8, 3, PixelType::kUInt8, DataLayout::kChannelsLast);

    // RGBA test image 6x4x4
    rgba_data_ = CreateImageData(6, 4, 4);
    rgba_dims_ =
        ImageInfo(6, 4, 4, PixelType::kUInt8, DataLayout::kChannelsLast);

    // Grayscale test image 20x15x1
    grayscale_data_ = CreateImageData(20, 15, 1);
    grayscale_dims_ =
        ImageInfo(20, 15, 1, PixelType::kUInt8, DataLayout::kChannelsLast);

    // Large test image 100x80x3
    large_data_ = CreateImageData(100, 80, 3);
    large_dims_ =
        ImageInfo(100, 80, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  }

  std::vector<uint8_t> CreateImageData(int width, int height, int channels) {
    std::vector<uint8_t> data(width * height * channels);

    // Fill with predictable pattern: value = (y * width + x + channel) % 256
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int pixel_offset = (y * width + x) * channels;
        for (int c = 0; c < channels; ++c) {
          data[pixel_offset + c] =
              static_cast<uint8_t>((y * width + x + c) % 256);
        }
      }
    }
    return data;
  }

 protected:
  std::vector<uint8_t> rgb_data_;
  ImageInfo rgb_dims_;
  std::vector<uint8_t> rgba_data_;
  ImageInfo rgba_dims_;
  std::vector<uint8_t> grayscale_data_;
  ImageInfo grayscale_dims_;
  std::vector<uint8_t> large_data_;
  ImageInfo large_dims_;
};

// Test basic construction
TEST_F(MemorySourceTest, BasicConstruction) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  auto dims = source.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 10);
  EXPECT_EQ(dims.GetHeight(), 8);
  EXPECT_EQ(dims.channels, 3);
}

// Test construction with custom tile size
TEST_F(MemorySourceTest, ConstructionWithCustomTileSize) {
  TileSize custom_tile_size(64, 64);
  MemorySource source = MemorySource::Create(std::vector<uint8_t>(rgb_data_),
                                             rgb_dims_, custom_tile_size);

  auto tile_size = source.GetIdealTileSize();
  EXPECT_EQ(tile_size.width, 64);
  EXPECT_EQ(tile_size.height, 64);
}

// Test dimensions with different channel counts
TEST_F(MemorySourceTest, DifferentChannelCounts) {
  MemorySource rgb_source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);
  MemorySource rgba_source =
      MemorySource::Create(std::vector<uint8_t>(rgba_data_), rgba_dims_);
  MemorySource grayscale_source = MemorySource::Create(
      std::vector<uint8_t>(grayscale_data_), grayscale_dims_);

  auto rgb_dims = rgb_source.GetDimensions();
  auto rgba_dims = rgba_source.GetDimensions();
  auto grayscale_dims = grayscale_source.GetDimensions();

  EXPECT_EQ(rgb_dims.channels, 3);
  EXPECT_EQ(rgba_dims.channels, 4);
  EXPECT_EQ(grayscale_dims.channels, 1);
}

// Test ideal tile size
TEST_F(MemorySourceTest, IdealTileSize) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  auto tile_size = source.GetIdealTileSize();
  EXPECT_EQ(tile_size.width, 256);  // Default tile size
  EXPECT_EQ(tile_size.height, 256);
}

// Test getting a full tile
TEST_F(MemorySourceTest, GetFullTile) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  auto tile = source.GetTile(0, 0, 10, 8);
  EXPECT_EQ(tile.x, 0);
  EXPECT_EQ(tile.y, 0);
  EXPECT_EQ(tile.width, 10);
  EXPECT_EQ(tile.height, 8);
  EXPECT_EQ(tile.channels, 3);
  EXPECT_EQ(tile.GetData().size(), 10 * 8 * 3);
}

// Test getting a partial tile
TEST_F(MemorySourceTest, GetPartialTile) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  auto tile = source.GetTile(2, 3, 4, 3);
  EXPECT_EQ(tile.x, 2);
  EXPECT_EQ(tile.y, 3);
  EXPECT_EQ(tile.width, 4);
  EXPECT_EQ(tile.height, 3);
  EXPECT_EQ(tile.channels, 3);
  EXPECT_EQ(tile.GetData().size(), 4 * 3 * 3);
}

// Test tile data correctness
TEST_F(MemorySourceTest, TileDataCorrectness) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Get a 2x2 tile from position (1, 1)
  auto tile = source.GetTile(1, 1, 2, 2);

  // Verify the data matches expected pattern
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      int source_x = 1 + x;
      int source_y = 1 + y;
      int tile_offset = (y * 2 + x) * 3;

      for (int c = 0; c < 3; ++c) {
        uint8_t expected =
            static_cast<uint8_t>((source_y * 10 + source_x + c) % 256);
        EXPECT_EQ(tile.GetData()[tile_offset + c], expected);
      }
    }
  }
}

// Test getting a tile beyond image bounds
TEST_F(MemorySourceTest, GetTileBeyondBounds) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  auto tile = source.GetTile(8, 6, 5, 5);
  EXPECT_EQ(tile.x, 8);
  EXPECT_EQ(tile.y, 6);
  EXPECT_EQ(tile.width, 2);   // Clamped to image bounds: 10 - 8 = 2
  EXPECT_EQ(tile.height, 2);  // Clamped to image bounds: 8 - 6 = 2
  EXPECT_EQ(tile.channels, 3);
}

// Test getting a tile completely outside image bounds
TEST_F(MemorySourceTest, GetTileOutsideBounds) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  auto tile = source.GetTile(15, 10, 5, 5);
  EXPECT_EQ(tile.x, 15);
  EXPECT_EQ(tile.y, 10);
  EXPECT_EQ(tile.width, 0);
  EXPECT_EQ(tile.height, 0);
  EXPECT_EQ(tile.channels, 3);
  EXPECT_TRUE(tile.GetData().empty());
}

// Test getting a tile with negative coordinates (should clamp and use padding)
TEST_F(MemorySourceTest, GetTileNegativeCoordinates) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Request: x=-5, y=-3, width=10, height=8
  // Valid region: (0,0) to (5,5) -> actual data is 5x5
  auto tile = source.GetTile(-5, -3, 10, 8);
  EXPECT_EQ(tile.x, -5);               // Original requested x
  EXPECT_EQ(tile.y, -3);               // Original requested y
  EXPECT_EQ(tile.width, 5);            // Actual valid width (0 to 5)
  EXPECT_EQ(tile.height, 5);           // Actual valid height (0 to 5)
  EXPECT_EQ(tile.expected_width, 10);  // Requested width
  EXPECT_EQ(tile.expected_height, 8);  // Requested height
  EXPECT_TRUE(tile.needs_padding);     // Padding is needed
  EXPECT_EQ(tile.channels, 3);

  // Should contain data from the top-left corner (0,0) of the source
  // Data dimensions are 5x5
  for (int y = 0; y < 5; ++y) {
    for (int x = 0; x < 5; ++x) {
      int tile_offset = (y * 5 + x) * 3;
      for (int c = 0; c < 3; ++c) {
        uint8_t expected = static_cast<uint8_t>((y * 10 + x + c) % 256);
        EXPECT_EQ(tile.GetData()[tile_offset + c], expected);
      }
    }
  }
}

// Test move constructor
TEST_F(MemorySourceTest, MoveConstructor) {
  MemorySource source1 =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);
  auto dims1 = source1.GetDimensions();
  auto memory_usage1 = source1.GetMemoryUsage();

  MemorySource source2(std::move(source1));
  auto dims2 = source2.GetDimensions();
  auto memory_usage2 = source2.GetMemoryUsage();

  EXPECT_EQ(dims2.GetWidth(), dims1.GetWidth());
  EXPECT_EQ(dims2.GetHeight(), dims1.GetHeight());
  EXPECT_EQ(dims2.channels, dims1.channels);
  EXPECT_EQ(memory_usage2, memory_usage1);

  // Verify source2 is functional
  auto tile = source2.GetTile(0, 0, 2, 2);
  EXPECT_EQ(tile.width, 2);
  EXPECT_EQ(tile.height, 2);
}

// Test move assignment
TEST_F(MemorySourceTest, MoveAssignment) {
  MemorySource source1 =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);
  MemorySource source2 =
      MemorySource::Create(std::vector<uint8_t>(rgba_data_), rgba_dims_);

  auto dims1 = source1.GetDimensions();
  auto memory_usage1 = source1.GetMemoryUsage();

  source2 = std::move(source1);
  auto dims2 = source2.GetDimensions();
  auto memory_usage2 = source2.GetMemoryUsage();

  EXPECT_EQ(dims2.GetWidth(), dims1.GetWidth());
  EXPECT_EQ(dims2.GetHeight(), dims1.GetHeight());
  EXPECT_EQ(dims2.channels, dims1.channels);
  EXPECT_EQ(memory_usage2, memory_usage1);
}

// Test memory usage calculation
TEST_F(MemorySourceTest, MemoryUsage) {
  MemorySource rgb_source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);
  MemorySource rgba_source =
      MemorySource::Create(std::vector<uint8_t>(rgba_data_), rgba_dims_);
  MemorySource grayscale_source = MemorySource::Create(
      std::vector<uint8_t>(grayscale_data_), grayscale_dims_);

  EXPECT_EQ(rgb_source.GetMemoryUsage(), 10 * 8 * 3);         // 240 bytes
  EXPECT_EQ(rgba_source.GetMemoryUsage(), 6 * 4 * 4);         // 96 bytes
  EXPECT_EQ(grayscale_source.GetMemoryUsage(), 20 * 15 * 1);  // 300 bytes
}

// Test large image handling
TEST_F(MemorySourceTest, LargeImage) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(large_data_), large_dims_);

  auto dims = source.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 100);
  EXPECT_EQ(dims.GetHeight(), 80);
  EXPECT_EQ(dims.channels, 3);

  // Test getting various tiles from large image
  auto corner_tile = source.GetTile(0, 0, 32, 32);
  EXPECT_EQ(corner_tile.width, 32);
  EXPECT_EQ(corner_tile.height, 32);

  auto center_tile = source.GetTile(40, 30, 20, 20);
  EXPECT_EQ(center_tile.width, 20);
  EXPECT_EQ(center_tile.height, 20);

  auto edge_tile = source.GetTile(90, 70, 20, 20);
  EXPECT_EQ(edge_tile.width, 10);   // Clamped
  EXPECT_EQ(edge_tile.height, 10);  // Clamped

  EXPECT_EQ(source.GetMemoryUsage(), 100 * 80 * 3);
}

// Test with empty image
TEST_F(MemorySourceTest, EmptyImage) {
  ImageInfo empty_dims(0, 0, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  std::vector<uint8_t> empty_data;
  MemorySource source = MemorySource::Create(std::move(empty_data), empty_dims);

  auto dims = source.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 0);
  EXPECT_EQ(dims.GetHeight(), 0);
  EXPECT_EQ(dims.channels, 3);

  auto tile = source.GetTile(0, 0, 10, 10);
  EXPECT_EQ(tile.width, 0);
  EXPECT_EQ(tile.height, 0);
  EXPECT_TRUE(tile.GetData().empty());

  EXPECT_EQ(source.GetMemoryUsage(), 0);
}

// Test single pixel image
TEST_F(MemorySourceTest, SinglePixelImage) {
  ImageInfo single_dims(1, 1, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  std::vector<uint8_t> single_data = {100, 150, 200};
  MemorySource source =
      MemorySource::Create(std::move(single_data), single_dims);

  auto dims = source.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 1);
  EXPECT_EQ(dims.GetHeight(), 1);
  EXPECT_EQ(dims.channels, 3);

  auto tile = source.GetTile(0, 0, 1, 1);
  EXPECT_EQ(tile.width, 1);
  EXPECT_EQ(tile.height, 1);
  EXPECT_EQ(tile.GetData().size(), 3);
  EXPECT_EQ(tile.GetData()[0], 100);
  EXPECT_EQ(tile.GetData()[1], 150);
  EXPECT_EQ(tile.GetData()[2], 200);
}

// Test grayscale image handling
TEST_F(MemorySourceTest, GrayscaleImage) {
  MemorySource source = MemorySource::Create(
      std::vector<uint8_t>(grayscale_data_), grayscale_dims_);

  auto dims = source.GetDimensions();
  EXPECT_EQ(dims.channels, 1);

  auto tile = source.GetTile(5, 5, 3, 3);
  EXPECT_EQ(tile.channels, 1);
  EXPECT_EQ(tile.GetData().size(), 3 * 3 * 1);

  // Verify grayscale data pattern
  for (int y = 0; y < 3; ++y) {
    for (int x = 0; x < 3; ++x) {
      int source_x = 5 + x;
      int source_y = 5 + y;
      int tile_offset = y * 3 + x;
      uint8_t expected = static_cast<uint8_t>((source_y * 20 + source_x) % 256);
      EXPECT_EQ(tile.GetData()[tile_offset], expected);
    }
  }
}

// Test RGBA image handling
TEST_F(MemorySourceTest, RgbaImage) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgba_data_), rgba_dims_);

  auto dims = source.GetDimensions();
  EXPECT_EQ(dims.channels, 4);

  auto tile = source.GetTile(1, 1, 2, 2);
  EXPECT_EQ(tile.channels, 4);
  EXPECT_EQ(tile.GetData().size(), 2 * 2 * 4);

  // Verify RGBA data pattern
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      int source_x = 1 + x;
      int source_y = 1 + y;
      int tile_offset = (y * 2 + x) * 4;

      for (int c = 0; c < 4; ++c) {
        uint8_t expected =
            static_cast<uint8_t>((source_y * 6 + source_x + c) % 256);
        EXPECT_EQ(tile.GetData()[tile_offset + c], expected);
      }
    }
  }
}

// Test multiple tile accesses
TEST_F(MemorySourceTest, MultipleTileAccess) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Access the same tile multiple times
  auto tile1 = source.GetTile(2, 2, 3, 3);
  auto tile2 = source.GetTile(2, 2, 3, 3);

  EXPECT_EQ(tile1.width, tile2.width);
  EXPECT_EQ(tile1.height, tile2.height);
  EXPECT_EQ(tile1.GetData(), tile2.GetData());

  // Access different tiles
  auto tile3 = source.GetTile(0, 0, 2, 2);
  auto tile4 = source.GetTile(5, 5, 2, 2);

  EXPECT_EQ(tile3.width, 2);
  EXPECT_EQ(tile4.width, 2);
  EXPECT_NE(tile3.GetData(), tile4.GetData());  // Should have different data
}

// Test tile partially outside on right edge
TEST_F(MemorySourceTest, GetTilePartiallyOutsideRight) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Request extends beyond right edge: x=8, width=5
  // Image width is 10, so valid data is from x=8 to x=10 (width=2)
  auto tile = source.GetTile(8, 2, 5, 3);
  EXPECT_EQ(tile.x, 8);
  EXPECT_EQ(tile.y, 2);
  EXPECT_EQ(tile.width, 2);            // Clipped to image bounds
  EXPECT_EQ(tile.height, 3);           // Height is fine
  EXPECT_EQ(tile.expected_width, 5);   // Original requested width
  EXPECT_EQ(tile.expected_height, 3);  // Original requested height
  EXPECT_TRUE(tile.needs_padding);
  EXPECT_EQ(tile.channels, 3);
}

// Test tile partially outside on bottom edge
TEST_F(MemorySourceTest, GetTilePartiallyOutsideBottom) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Request extends beyond bottom edge: y=7, height=5
  // Image height is 8, so valid data is from y=7 to y=8 (height=1)
  auto tile = source.GetTile(2, 7, 4, 5);
  EXPECT_EQ(tile.x, 2);
  EXPECT_EQ(tile.y, 7);
  EXPECT_EQ(tile.width, 4);            // Width is fine
  EXPECT_EQ(tile.height, 1);           // Clipped to image bounds
  EXPECT_EQ(tile.expected_width, 4);   // Original requested width
  EXPECT_EQ(tile.expected_height, 5);  // Original requested height
  EXPECT_TRUE(tile.needs_padding);
  EXPECT_EQ(tile.channels, 3);
}

// Test tile partially outside on left edge only
TEST_F(MemorySourceTest, GetTilePartiallyOutsideLeft) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Request starts before left edge: x=-3, width=8
  // Valid data is from x=0 to x=5 (width=5)
  auto tile = source.GetTile(-3, 2, 8, 3);
  EXPECT_EQ(tile.x, -3);
  EXPECT_EQ(tile.y, 2);
  EXPECT_EQ(tile.width, 5);            // From 0 to (-3+8)=5
  EXPECT_EQ(tile.height, 3);           // Height is fine
  EXPECT_EQ(tile.expected_width, 8);   // Original requested width
  EXPECT_EQ(tile.expected_height, 3);  // Original requested height
  EXPECT_TRUE(tile.needs_padding);
  EXPECT_EQ(tile.channels, 3);
}

// Test tile partially outside on top edge only
TEST_F(MemorySourceTest, GetTilePartiallyOutsideTop) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Request starts before top edge: y=-2, height=6
  // Valid data is from y=0 to y=4 (height=4)
  auto tile = source.GetTile(2, -2, 3, 6);
  EXPECT_EQ(tile.x, 2);
  EXPECT_EQ(tile.y, -2);
  EXPECT_EQ(tile.width, 3);            // Width is fine
  EXPECT_EQ(tile.height, 4);           // From 0 to (-2+6)=4
  EXPECT_EQ(tile.expected_width, 3);   // Original requested width
  EXPECT_EQ(tile.expected_height, 6);  // Original requested height
  EXPECT_TRUE(tile.needs_padding);
  EXPECT_EQ(tile.channels, 3);
}

// Test tile entirely outside bounds (far right)
TEST_F(MemorySourceTest, GetTileEntirelyOutsideRight) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Request completely beyond image: x=20, width=5
  // Image width is 10
  auto tile = source.GetTile(20, 2, 5, 3);
  EXPECT_EQ(tile.x, 20);
  EXPECT_EQ(tile.y, 2);
  EXPECT_EQ(tile.width, 0);
  EXPECT_EQ(tile.height, 0);
  EXPECT_EQ(tile.expected_width, 5);
  EXPECT_EQ(tile.expected_height, 3);
  EXPECT_EQ(tile.channels, 3);
  EXPECT_TRUE(tile.GetData().empty());
}

// Test tile with multiple edges clipped (bottom-right corner)
TEST_F(MemorySourceTest, GetTileMultipleEdgesClipped) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Request extends beyond both right and bottom: x=8, y=7, width=5, height=6
  // Image is 10x8, so valid data is from (8,7) to (10,8) -> 2x1
  auto tile = source.GetTile(8, 7, 5, 6);
  EXPECT_EQ(tile.x, 8);
  EXPECT_EQ(tile.y, 7);
  EXPECT_EQ(tile.width, 2);   // Clipped in x
  EXPECT_EQ(tile.height, 1);  // Clipped in y
  EXPECT_EQ(tile.expected_width, 5);
  EXPECT_EQ(tile.expected_height, 6);
  EXPECT_TRUE(tile.needs_padding);
  EXPECT_EQ(tile.channels, 3);
}

// Test tile that fits exactly (no padding needed)
TEST_F(MemorySourceTest, GetTileExactFit) {
  MemorySource source =
      MemorySource::Create(std::vector<uint8_t>(rgb_data_), rgb_dims_);

  // Request that fits perfectly within bounds
  auto tile = source.GetTile(2, 3, 4, 5);
  EXPECT_EQ(tile.x, 2);
  EXPECT_EQ(tile.y, 3);
  EXPECT_EQ(tile.width, 4);
  EXPECT_EQ(tile.height, 5);
  EXPECT_EQ(tile.expected_width, 0);  // No padding info set
  EXPECT_EQ(tile.expected_height, 0);
  EXPECT_FALSE(tile.needs_padding);  // No padding needed
  EXPECT_EQ(tile.channels, 3);
}

// Test uint16 typed memory source
TEST_F(MemorySourceTest, UInt16TypedSource) {
  std::vector<uint16_t> uint16_data(10 * 8 * 3);
  for (size_t i = 0; i < uint16_data.size(); ++i) {
    uint16_data[i] = static_cast<uint16_t>(i * 100);
  }

  MemorySource source =
      MemorySource::CreateTyped(std::move(uint16_data), 10, 8, 3);

  auto dims = source.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 10);
  EXPECT_EQ(dims.GetHeight(), 8);
  EXPECT_EQ(dims.channels, 3);
  EXPECT_EQ(dims.pixel_type, PixelType::kUInt16);

  // Memory usage should be 2 bytes per element
  EXPECT_EQ(source.GetMemoryUsage(), 10 * 8 * 3 * 2);

  // Get a tile and verify data
  auto tile = source.GetTile(0, 0, 2, 2);
  EXPECT_EQ(tile.GetPixelType(), PixelType::kUInt16);
  EXPECT_EQ(tile.width, 2);
  EXPECT_EQ(tile.height, 2);
  EXPECT_EQ(tile.channels, 3);

  // Verify data access
  const auto& tile_data = tile.GetDataAs<uint16_t>();
  EXPECT_EQ(tile_data.size(), 2 * 2 * 3);
  EXPECT_EQ(tile_data[0], 0);    // (0,0) channel 0
  EXPECT_EQ(tile_data[1], 100);  // (0,0) channel 1
  EXPECT_EQ(tile_data[2], 200);  // (0,0) channel 2
  EXPECT_EQ(tile_data[3], 300);  // (0,1) channel 0
}

// Test float32 typed memory source
TEST_F(MemorySourceTest, Float32TypedSource) {
  std::vector<float> float_data(5 * 4 * 1);
  for (size_t i = 0; i < float_data.size(); ++i) {
    float_data[i] = static_cast<float>(i) * 0.5f;
  }

  MemorySource source =
      MemorySource::CreateTyped(std::move(float_data), 5, 4, 1);

  auto dims = source.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 5);
  EXPECT_EQ(dims.GetHeight(), 4);
  EXPECT_EQ(dims.channels, 1);
  EXPECT_EQ(dims.pixel_type, PixelType::kFloat32);

  // Memory usage should be 4 bytes per element
  EXPECT_EQ(source.GetMemoryUsage(), 5 * 4 * 1 * 4);

  // Get a tile and verify data
  auto tile = source.GetTile(1, 1, 2, 2);
  EXPECT_EQ(tile.GetPixelType(), PixelType::kFloat32);
  EXPECT_EQ(tile.width, 2);
  EXPECT_EQ(tile.height, 2);

  const auto& tile_data = tile.GetDataAs<float>();
  EXPECT_EQ(tile_data.size(), 2 * 2 * 1);
  // Pixel at (1,1) is at index (1*5+1)*1 = 6 in source
  EXPECT_FLOAT_EQ(tile_data[0], 6.0f * 0.5f);   // (1,1)
  EXPECT_FLOAT_EQ(tile_data[1], 7.0f * 0.5f);   // (1,2)
  EXPECT_FLOAT_EQ(tile_data[2], 11.0f * 0.5f);  // (2,1)
  EXPECT_FLOAT_EQ(tile_data[3], 12.0f * 0.5f);  // (2,2)
}

// Test uint16 to FImage round-trip
TEST_F(MemorySourceTest, UInt16FImageRoundTrip) {
  std::vector<uint16_t> uint16_data(4 * 3 * 2);
  for (size_t i = 0; i < uint16_data.size(); ++i) {
    uint16_data[i] = static_cast<uint16_t>(i * 1000);
  }

  MemorySource source =
      MemorySource::CreateTyped(std::move(uint16_data), 4, 3, 2);

  auto dims = source.GetDimensions();
  EXPECT_EQ(dims.pixel_type, PixelType::kUInt16);

  // Get full tile
  auto tile = source.GetTile(0, 0, 4, 3);
  EXPECT_EQ(tile.GetPixelType(), PixelType::kUInt16);

  const auto& data = tile.GetDataAs<uint16_t>();
  EXPECT_EQ(data.size(), 4 * 3 * 2);
  EXPECT_EQ(data[0], 0);
  EXPECT_EQ(data[1], 1000);
  EXPECT_EQ(data[2], 2000);
}

// Test undersized buffer validation for kUInt8
TEST_F(MemorySourceTest, UndersizedBufferUInt8) {
  // Create dimensions requiring 100 bytes (10x10x1)
  ImageInfo dims(10, 10, 1, PixelType::kUInt8, DataLayout::kChannelsLast);

  // Provide only 50 bytes
  std::vector<uint8_t> undersized_buffer(50);

  EXPECT_THROW(
      { MemorySource::Create(std::move(undersized_buffer), dims); },
      std::invalid_argument);
}

// Test undersized buffer validation for kUInt16
TEST_F(MemorySourceTest, UndersizedBufferUInt16) {
  // Create dimensions requiring 200 bytes (10x10x1 uint16 = 10*10*2)
  ImageInfo dims(10, 10, 1, PixelType::kUInt16, DataLayout::kChannelsLast);

  // Provide only 100 bytes (half of what's needed)
  std::vector<uint8_t> undersized_buffer(100);

  EXPECT_THROW(
      { MemorySource::Create(std::move(undersized_buffer), dims); },
      std::invalid_argument);
}

// Test undersized buffer validation for kFloat32
TEST_F(MemorySourceTest, UndersizedBufferFloat32) {
  // Create dimensions requiring 120 bytes (10x3x1 float = 10*3*4)
  ImageInfo dims(10, 3, 1, PixelType::kFloat32, DataLayout::kChannelsLast);

  // Provide only 60 bytes (half of what's needed)
  std::vector<uint8_t> undersized_buffer(60);

  EXPECT_THROW(
      { MemorySource::Create(std::move(undersized_buffer), dims); },
      std::invalid_argument);
}

// Test exactly sized buffer (should not throw)
TEST_F(MemorySourceTest, ExactlySizedBuffer) {
  ImageInfo dims(5, 4, 3, PixelType::kUInt16, DataLayout::kChannelsLast);

  // Create exactly the right size buffer (5*4*3*2 = 120 bytes)
  std::vector<uint8_t> exact_buffer(120, 0);

  // Should not throw
  EXPECT_NO_THROW({ MemorySource::Create(std::move(exact_buffer), dims); });
}

// Test oversized buffer (should be fine, extra bytes ignored)
TEST_F(MemorySourceTest, OversizedBuffer) {
  ImageInfo dims(5, 4, 3, PixelType::kUInt8, DataLayout::kChannelsLast);

  // Create a buffer larger than needed (60 needed, 100 provided)
  std::vector<uint8_t> oversized_buffer(100, 0);

  // Should not throw, extra bytes are ignored
  EXPECT_NO_THROW({ MemorySource::Create(std::move(oversized_buffer), dims); });
}

}  // namespace fim

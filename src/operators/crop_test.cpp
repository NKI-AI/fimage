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

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "fim/operators/crop.h"
#include "fim/types.h"

namespace fim {

// Mock source for testing operators
class MockSource {
 public:
  MockSource(int width, int height, int channels)
      : dims_(width, height, channels, PixelType::kUInt8,
              DataLayout::kChannelsLast),
        tile_size_(256, 256) {}

  ImageInfo GetDimensions() const { return dims_; }

  TileSize GetIdealTileSize() const { return tile_size_; }

  Tile GetTile(int x, int y, int width, int height) const {
    // Clamp to image bounds
    int actual_width = std::min(width, dims_.GetWidth() - x);
    int actual_height = std::min(height, dims_.GetHeight() - y);

    if (actual_width <= 0 || actual_height <= 0) {
      return Tile(x, y, 0, 0, dims_.channels);
    }

    Tile tile(x, y, actual_width, actual_height, dims_.channels);

    // Fill with test pattern: pixel value = (y * width + x) % 256
    for (int tile_y = 0; tile_y < actual_height; ++tile_y) {
      for (int tile_x = 0; tile_x < actual_width; ++tile_x) {
        int global_x = x + tile_x;
        int global_y = y + tile_y;
        uint8_t value = static_cast<uint8_t>(
            (global_y * dims_.GetWidth() + global_x) % 256);

        int pixel_offset = (tile_y * actual_width + tile_x) * dims_.channels;
        for (int c = 0; c < dims_.channels; ++c) {
          tile.GetDataMut()[pixel_offset + c] = value;
        }
      }
    }

    return tile;
  }

 private:
  ImageInfo dims_;
  TileSize tile_size_;
};

class CropTest : public ::testing::Test {
 protected:
  void SetUp() override { source_ = std::make_unique<MockSource>(100, 100, 3); }

  void TearDown() override { source_.reset(); }

  std::unique_ptr<MockSource> source_;
};

// Test basic crop functionality
TEST_F(CropTest, BasicCrop) {
  Crop crop(*source_, 10, 20, 30, 40);

  auto dims = crop.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 30);
  EXPECT_EQ(dims.GetHeight(), 40);
  EXPECT_EQ(dims.channels, 3);
}

// Test crop with zero dimensions
TEST_F(CropTest, ZeroDimensions) {
  Crop crop(*source_, 10, 20, 0, 0);

  auto dims = crop.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 0);
  EXPECT_EQ(dims.GetHeight(), 0);
  EXPECT_EQ(dims.channels, 3);
}

// Test crop exceeding source bounds
TEST_F(CropTest, ExceedingSourceBounds) {
  Crop crop(*source_, 50, 60, 100, 100);

  auto dims = crop.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 50);   // 100 - 50 = 50
  EXPECT_EQ(dims.GetHeight(), 40);  // 100 - 60 = 40
  EXPECT_EQ(dims.channels, 3);
}

// Test crop starting outside source bounds
TEST_F(CropTest, StartingOutsideSourceBounds) {
  Crop crop(*source_, 150, 200, 50, 50);

  auto dims = crop.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 0);
  EXPECT_EQ(dims.GetHeight(), 0);
  EXPECT_EQ(dims.channels, 3);
}

// Test crop with negative coordinates (should be handled gracefully)
TEST_F(CropTest, NegativeCoordinates) {
  Crop crop(*source_, -10, -20, 30, 40);

  auto dims = crop.GetDimensions();
  // Crop operator allows negative coordinates and extends the valid region
  // actual_width = min(30, 100 - (-10)) = min(30, 110) = 30
  // actual_height = min(40, 100 - (-20)) = min(40, 120) = 40
  EXPECT_EQ(dims.GetWidth(), 30);
  EXPECT_EQ(dims.GetHeight(), 40);
  EXPECT_EQ(dims.channels, 3);
}

// Test ideal tile size propagation
TEST_F(CropTest, IdealTileSize) {
  Crop crop(*source_, 10, 20, 30, 40);

  auto tile_size = crop.GetIdealTileSize();
  EXPECT_EQ(tile_size.width, 256);
  EXPECT_EQ(tile_size.height, 256);
}

// Test getting a tile from cropped region
TEST_F(CropTest, GetTile) {
  Crop crop(*source_, 10, 20, 30, 40);

  auto tile = crop.GetTile(5, 5, 10, 10);
  EXPECT_EQ(tile.x, 5);
  EXPECT_EQ(tile.y, 5);
  EXPECT_EQ(tile.width, 10);
  EXPECT_EQ(tile.height, 10);
  EXPECT_EQ(tile.channels, 3);

  // Verify data is from the correct source region
  // The tile should contain data from source region (15, 25) to (24, 34)
  for (int y = 0; y < 10; ++y) {
    for (int x = 0; x < 10; ++x) {
      int source_x = 15 + x;  // 10 (crop start) + 5 (tile start) + x
      int source_y = 25 + y;  // 20 (crop start) + 5 (tile start) + y
      uint8_t expected_value =
          static_cast<uint8_t>((source_y * 100 + source_x) % 256);

      int pixel_offset = (y * 10 + x) * 3;
      for (int c = 0; c < 3; ++c) {
        EXPECT_EQ(tile.GetData()[pixel_offset + c], expected_value);
      }
    }
  }
}

// Test getting a tile that extends beyond crop bounds
TEST_F(CropTest, GetTileBeyondCropBounds) {
  Crop crop(*source_, 10, 20, 30, 40);

  auto tile = crop.GetTile(25, 35, 10, 10);
  EXPECT_EQ(tile.x, 25);
  EXPECT_EQ(tile.y, 35);
  EXPECT_EQ(tile.width, 5);   // Clamped to crop bounds
  EXPECT_EQ(tile.height, 5);  // Clamped to crop bounds
  EXPECT_EQ(tile.channels, 3);
}

// Test getting a tile completely outside crop bounds
TEST_F(CropTest, GetTileOutsideCropBounds) {
  Crop crop(*source_, 10, 20, 30, 40);

  auto tile = crop.GetTile(50, 60, 10, 10);
  EXPECT_EQ(tile.x, 50);
  EXPECT_EQ(tile.y, 60);
  EXPECT_EQ(tile.width, 0);
  EXPECT_EQ(tile.height, 0);
  EXPECT_EQ(tile.channels, 3);
}

// Test crop of full image
TEST_F(CropTest, FullImageCrop) {
  Crop crop(*source_, 0, 0, 100, 100);

  auto dims = crop.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 100);
  EXPECT_EQ(dims.GetHeight(), 100);
  EXPECT_EQ(dims.channels, 3);

  // Getting a tile should work exactly like the source
  auto tile = crop.GetTile(10, 10, 20, 20);
  EXPECT_EQ(tile.width, 20);
  EXPECT_EQ(tile.height, 20);
}

// Test crop with different channel counts
TEST_F(CropTest, DifferentChannelCounts) {
  MockSource grayscale_source(50, 50, 1);
  MockSource rgba_source(50, 50, 4);

  Crop grayscale_crop(grayscale_source, 10, 10, 20, 20);
  Crop rgba_crop(rgba_source, 10, 10, 20, 20);

  auto grayscale_dims = grayscale_crop.GetDimensions();
  auto rgba_dims = rgba_crop.GetDimensions();

  EXPECT_EQ(grayscale_dims.channels, 1);
  EXPECT_EQ(rgba_dims.channels, 4);
}

}  // namespace fim

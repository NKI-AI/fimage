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

#include "fim/operators/downsample.h"
#include "fim/types.h"

namespace fim {

// Mock source for testing operators
class MockSource {
 public:
  MockSource(int width, int height, int channels, uint8_t fill_value = 100)
      : dims_(width, height, channels, PixelType::kUInt8,
              DataLayout::kChannelsLast),
        tile_size_(256, 256),
        fill_value_(fill_value) {}

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

    // Fill with test pattern based on fill_value_
    for (int tile_y = 0; tile_y < actual_height; ++tile_y) {
      for (int tile_x = 0; tile_x < actual_width; ++tile_x) {
        int global_x = x + tile_x;
        int global_y = y + tile_y;
        uint8_t value = static_cast<uint8_t>(
            (fill_value_ + global_y * dims_.GetWidth() + global_x) % 256);

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
  uint8_t fill_value_;
};

class DownsampleTest : public ::testing::Test {
 protected:
  void SetUp() override { source_ = std::make_unique<MockSource>(100, 100, 3); }

  void TearDown() override { source_.reset(); }

  std::unique_ptr<MockSource> source_;
};

// Test basic downsample functionality
TEST_F(DownsampleTest, BasicDownsample) {
  Downsample downsample(*source_, 2);

  auto dims = downsample.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 50);   // 100 / 2
  EXPECT_EQ(dims.GetHeight(), 50);  // 100 / 2
  EXPECT_EQ(dims.channels, 3);
}

// Test downsample with factor 1 (no change)
TEST_F(DownsampleTest, NoDownsample) {
  Downsample downsample(*source_, 1);

  auto dims = downsample.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 100);
  EXPECT_EQ(dims.GetHeight(), 100);
  EXPECT_EQ(dims.channels, 3);
}

// Test downsample with factor 4
TEST_F(DownsampleTest, DownsampleBy4) {
  Downsample downsample(*source_, 4);

  auto dims = downsample.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 25);   // 100 / 4
  EXPECT_EQ(dims.GetHeight(), 25);  // 100 / 4
  EXPECT_EQ(dims.channels, 3);
}

// Test downsample with non-divisible dimensions
TEST_F(DownsampleTest, NonDivisibleDimensions) {
  MockSource odd_source(101, 103, 3);
  Downsample downsample(odd_source, 2);

  auto dims = downsample.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 51);   // (101 + 2 - 1) / 2 = 51
  EXPECT_EQ(dims.GetHeight(), 52);  // (103 + 2 - 1) / 2 = 52
  EXPECT_EQ(dims.channels, 3);
}

// Test downsample with invalid factor (should throw)
TEST_F(DownsampleTest, InvalidFactor) {
  EXPECT_THROW(Downsample(*source_, 0), std::invalid_argument);
  EXPECT_THROW(Downsample(*source_, -1), std::invalid_argument);
}

// Test ideal tile size calculation
TEST_F(DownsampleTest, IdealTileSize) {
  Downsample downsample(*source_, 2);

  auto tile_size = downsample.GetIdealTileSize();
  EXPECT_EQ(tile_size.width, 128);   // 256 / 2
  EXPECT_EQ(tile_size.height, 128);  // 256 / 2
}

// Test ideal tile size with factor 1
TEST_F(DownsampleTest, IdealTileSizeNoDownsample) {
  Downsample downsample(*source_, 1);

  auto tile_size = downsample.GetIdealTileSize();
  EXPECT_EQ(tile_size.width, 256);
  EXPECT_EQ(tile_size.height, 256);
}

// Test getting a tile from downsampled output
TEST_F(DownsampleTest, GetTile) {
  // Create a source with predictable values
  MockSource uniform_source(4, 4, 1,
                            0);  // 4x4 image, 1 channel, fill starting at 0
  Downsample downsample(uniform_source, 2);

  auto tile = downsample.GetTile(0, 0, 2, 2);
  EXPECT_EQ(tile.x, 0);
  EXPECT_EQ(tile.y, 0);
  EXPECT_EQ(tile.width, 2);
  EXPECT_EQ(tile.height, 2);
  EXPECT_EQ(tile.channels, 1);

  // Each output pixel should be the average of 2x2 input pixels
  // For a 4x4 input with values [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]
  // The 2x2 downsampled output should be:
  // Top-left: average of input pixels (0,0), (0,1), (1,0), (1,1) = (0+1+4+5)/4
  // = 2.5 ≈ 2 Top-right: average of input pixels (0,2), (0,3), (1,2), (1,3) =
  // (2+3+6+7)/4 = 4.5 ≈ 4 Bottom-left: average of input pixels (2,0), (2,1),
  // (3,0), (3,1) = (8+9+12+13)/4 = 10.5 ≈ 10 Bottom-right: average of input
  // pixels (2,2), (2,3), (3,2), (3,3) = (10+11+14+15)/4 = 12.5 ≈ 12

  EXPECT_EQ(tile.GetData()[0], 2);   // Top-left pixel
  EXPECT_EQ(tile.GetData()[1], 4);   // Top-right pixel
  EXPECT_EQ(tile.GetData()[2], 10);  // Bottom-left pixel
  EXPECT_EQ(tile.GetData()[3], 12);  // Bottom-right pixel
}

// Test getting a tile at non-zero position
TEST_F(DownsampleTest, GetTileNonZeroPosition) {
  Downsample downsample(*source_, 2);

  auto tile = downsample.GetTile(5, 10, 15, 20);
  EXPECT_EQ(tile.x, 5);
  EXPECT_EQ(tile.y, 10);
  EXPECT_EQ(tile.width, 15);
  EXPECT_EQ(tile.height, 20);
  EXPECT_EQ(tile.channels, 3);
}

// Test getting a tile that extends beyond output bounds
TEST_F(DownsampleTest, GetTileBeyondBounds) {
  Downsample downsample(*source_, 2);  // 100x100 -> 50x50

  auto tile = downsample.GetTile(40, 40, 20, 20);
  EXPECT_EQ(tile.x, 40);
  EXPECT_EQ(tile.y, 40);
  EXPECT_EQ(tile.width, 10);   // Clamped to output bounds: 50 - 40 = 10
  EXPECT_EQ(tile.height, 10);  // Clamped to output bounds: 50 - 40 = 10
  EXPECT_EQ(tile.channels, 3);
}

// Test getting a tile completely outside output bounds
TEST_F(DownsampleTest, GetTileOutsideBounds) {
  Downsample downsample(*source_, 2);  // 100x100 -> 50x50

  auto tile = downsample.GetTile(60, 70, 10, 10);
  EXPECT_EQ(tile.x, 60);
  EXPECT_EQ(tile.y, 70);
  EXPECT_EQ(tile.width, 0);
  EXPECT_EQ(tile.height, 0);
  EXPECT_EQ(tile.channels, 3);
}

// Test downsample with different channel counts
TEST_F(DownsampleTest, DifferentChannelCounts) {
  MockSource grayscale_source(50, 50, 1);
  MockSource rgba_source(50, 50, 4);

  Downsample grayscale_downsample(grayscale_source, 2);
  Downsample rgba_downsample(rgba_source, 2);

  auto grayscale_dims = grayscale_downsample.GetDimensions();
  auto rgba_dims = rgba_downsample.GetDimensions();

  EXPECT_EQ(grayscale_dims.channels, 1);
  EXPECT_EQ(rgba_dims.channels, 4);
}

// Test downsample with small image
TEST_F(DownsampleTest, SmallImage) {
  MockSource small_source(3, 3, 1);
  Downsample downsample(small_source, 2);

  auto dims = downsample.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 2);   // (3 + 2 - 1) / 2 = 2
  EXPECT_EQ(dims.GetHeight(), 2);  // (3 + 2 - 1) / 2 = 2
  EXPECT_EQ(dims.channels, 1);
}

// Test downsample with large factor
TEST_F(DownsampleTest, LargeFactor) {
  Downsample downsample(*source_, 10);

  auto dims = downsample.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 10);   // 100 / 10
  EXPECT_EQ(dims.GetHeight(), 10);  // 100 / 10
  EXPECT_EQ(dims.channels, 3);
}

}  // namespace fim

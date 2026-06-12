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
/**
 * @file zero_tile_test.cpp
 * @brief Tests for preventing zero-sized tiles in downsample operations.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains tests to verify that the downsample operator
 * properly handles cases where the input tile size is smaller than
 * the downsample factor, preventing zero-sized tiles.
 */

#include <gtest/gtest.h>

#include "fim/operators/downsample.h"
#include "fim/sources/memory_source.h"

namespace fim {
namespace {

/**
 * @brief Custom source with very small tile size.
 *
 * This source is used to test edge cases where the preferred tile
 * size is smaller than the downsample factor.
 */
class SmallTileSource : public SourceBase<SmallTileSource> {
 public:
  SmallTileSource(std::vector<uint8_t> data, ImageInfo dims, int tile_size)
      : data_(std::move(data)), dims_(dims), tile_size_(tile_size) {}

  ImageInfo GetDimensions() const { return dims_; }

  TileSize GetIdealTileSize() const { return TileSize(tile_size_, tile_size_); }

  Tile GetTile(int x, int y, int width, int height) const {
    int actual_width = std::min(width, dims_.GetWidth() - x);
    int actual_height = std::min(height, dims_.GetHeight() - y);

    if (actual_width <= 0 || actual_height <= 0) {
      return Tile(x, y, 0, 0, dims_.channels);
    }

    Tile tile(x, y, actual_width, actual_height, dims_.channels);

    for (int ty = 0; ty < actual_height; ++ty) {
      for (int tx = 0; tx < actual_width; ++tx) {
        for (int c = 0; c < dims_.channels; ++c) {
          int src_idx =
              ((y + ty) * dims_.GetWidth() + (x + tx)) * dims_.channels + c;
          int dst_idx = (ty * actual_width + tx) * dims_.channels + c;
          tile.GetDataMut()[dst_idx] = data_[src_idx];
        }
      }
    }

    return tile;
  }

 private:
  std::vector<uint8_t> data_;
  ImageInfo dims_;
  int tile_size_;
};

/**
 * @brief Test that downsample with tile size 1 doesn't produce zero tiles.
 *
 * This test verifies that when the input tile size is 1 and the downsample
 * factor is larger, the output tile size is clamped to at least 1.
 */
TEST(ZeroTileTest, TileSize1WithLargeDownsampleFactor) {
  // Create a small source with tile size of 1
  std::vector<uint8_t> data(16 * 16 * 1, 100);
  ImageInfo dims(16, 16, 1, PixelType::kUInt8, DataLayout::kChannelsLast);
  SmallTileSource source(std::move(data), dims, 1);

  // Downsample by factor of 4 (larger than tile size)
  Downsample<SmallTileSource> downsample(std::move(source), 4);

  // Verify the ideal tile size is at least 1x1
  auto tile_size = downsample.GetIdealTileSize();
  EXPECT_GE(tile_size.width, 1);
  EXPECT_GE(tile_size.height, 1);

  // Verify we can still get tiles
  auto tile = downsample.GetTile(0, 0, 1, 1);
  EXPECT_GE(tile.width, 1);
  EXPECT_GE(tile.height, 1);
}

/**
 * @brief Test that downsample with tile size 2 and factor 4 works correctly.
 *
 * This test verifies ceiling division: (2 + 4 - 1) / 4 = 1, not 0.
 */
TEST(ZeroTileTest, TileSizeSmallerThanFactor) {
  // Create a source with tile size of 2
  std::vector<uint8_t> data(32 * 32 * 3, 150);
  ImageInfo dims(32, 32, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  SmallTileSource source(std::move(data), dims, 2);

  // Downsample by factor of 4
  Downsample<SmallTileSource> downsample(std::move(source), 4);

  // Verify the ideal tile size uses ceiling division and is at least 1
  auto tile_size = downsample.GetIdealTileSize();
  EXPECT_EQ(tile_size.width, 1);  // (2 + 4 - 1) / 4 = 1
  EXPECT_EQ(tile_size.height, 1);

  // Verify the output dimensions
  auto dims_out = downsample.GetDimensions();
  EXPECT_EQ(dims_out.GetWidth(), 8);  // (32 + 4 - 1) / 4 = 8
  EXPECT_EQ(dims_out.GetHeight(), 8);
}

/**
 * @brief Test that downsample with tile size 3 and factor 2 rounds up.
 *
 * This test verifies ceiling division: (3 + 2 - 1) / 2 = 2, not 1.
 */
TEST(ZeroTileTest, CeilingDivisionCorrect) {
  // Create a source with tile size of 3
  std::vector<uint8_t> data(24 * 24 * 1, 200);
  ImageInfo dims(24, 24, 1, PixelType::kUInt8, DataLayout::kChannelsLast);
  SmallTileSource source(std::move(data), dims, 3);

  // Downsample by factor of 2
  Downsample<SmallTileSource> downsample(std::move(source), 2);

  // Verify the ideal tile size uses ceiling division
  auto tile_size = downsample.GetIdealTileSize();
  EXPECT_EQ(tile_size.width, 2);  // (3 + 2 - 1) / 2 = 2
  EXPECT_EQ(tile_size.height, 2);
}

/**
 * @brief Test that normal-sized tiles still work correctly.
 *
 * This test verifies that the ceiling division doesn't break the
 * common case where tile size is larger than the downsample factor.
 */
TEST(ZeroTileTest, NormalTileSizesUnaffected) {
  // Create a source with tile size of 256 (typical)
  std::vector<uint8_t> data(512 * 512 * 3, 50);
  ImageInfo dims(512, 512, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  SmallTileSource source(std::move(data), dims, 256);

  // Downsample by factor of 2
  Downsample<SmallTileSource> downsample(std::move(source), 2);

  // Verify the ideal tile size
  auto tile_size = downsample.GetIdealTileSize();
  EXPECT_EQ(tile_size.width, 128);  // (256 + 2 - 1) / 2 = 128
  EXPECT_EQ(tile_size.height, 128);

  // Verify we can get tiles with the recommended size
  auto tile = downsample.GetTile(0, 0, tile_size.width, tile_size.height);
  EXPECT_EQ(tile.width, 128);
  EXPECT_EQ(tile.height, 128);
}

/**
 * @brief Test that very small images with large downsample factors work.
 *
 * This is an extreme edge case: 4x4 image downsampled by 8.
 */
TEST(ZeroTileTest, VerySmallImageLargeDownsample) {
  // Create a very small source
  std::vector<uint8_t> data(4 * 4 * 1, 75);
  ImageInfo dims(4, 4, 1, PixelType::kUInt8, DataLayout::kChannelsLast);
  SmallTileSource source(std::move(data), dims, 2);

  // Downsample by factor of 8 (larger than image size)
  Downsample<SmallTileSource> downsample(std::move(source), 8);

  // Verify the output dimensions (ceiling division)
  auto dims_out = downsample.GetDimensions();
  EXPECT_EQ(dims_out.GetWidth(), 1);  // (4 + 8 - 1) / 8 = 1
  EXPECT_EQ(dims_out.GetHeight(), 1);

  // Verify the ideal tile size is at least 1
  auto tile_size = downsample.GetIdealTileSize();
  EXPECT_GE(tile_size.width, 1);
  EXPECT_GE(tile_size.height, 1);

  // Verify we can get the single output pixel
  auto tile = downsample.GetTile(0, 0, 1, 1);
  EXPECT_EQ(tile.width, 1);
  EXPECT_EQ(tile.height, 1);
}

/**
 * @brief Test odd tile sizes with various downsample factors.
 *
 * This test verifies that odd-sized tiles are handled correctly
 * with the ceiling division formula.
 */
TEST(ZeroTileTest, OddTileSizes) {
  std::vector<std::pair<int, int>> test_cases = {
      {5, 2},   // (5 + 2 - 1) / 2 = 3
      {7, 3},   // (7 + 3 - 1) / 3 = 3
      {11, 5},  // (11 + 5 - 1) / 5 = 3
      {1, 1},   // (1 + 1 - 1) / 1 = 1
  };

  for (const auto& [tile_size, factor] : test_cases) {
    std::vector<uint8_t> data(64 * 64 * 1, 100);
    ImageInfo dims(64, 64, 1, PixelType::kUInt8, DataLayout::kChannelsLast);
    SmallTileSource source(std::move(data), dims, tile_size);

    Downsample<SmallTileSource> downsample(std::move(source), factor);

    auto out_tile_size = downsample.GetIdealTileSize();

    // Verify it's at least 1
    EXPECT_GE(out_tile_size.width, 1)
        << "Failed for tile_size=" << tile_size << ", factor=" << factor;
    EXPECT_GE(out_tile_size.height, 1)
        << "Failed for tile_size=" << tile_size << ", factor=" << factor;

    // Verify it matches ceiling division
    int expected = std::max(1, (tile_size + factor - 1) / factor);
    EXPECT_EQ(out_tile_size.width, expected)
        << "Failed for tile_size=" << tile_size << ", factor=" << factor;
  }
}

}  // namespace
}  // namespace fim

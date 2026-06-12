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
 * @file black_source_test.cpp
 * @brief Tests for BlackSource class.
 * @author Jonas Teuwen
 * @date 2025
 */

#include "fim/sources/black_source.h"

#include <gtest/gtest.h>

namespace fim {
namespace {

TEST(BlackSourceTest, CreateValidSource) {
  ImageInfo dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto source = BlackSource::Create(dims);

  auto result_dims = source.GetDimensions();
  EXPECT_EQ(result_dims.GetWidth(), 100);
  EXPECT_EQ(result_dims.GetHeight(), 100);
  EXPECT_EQ(result_dims.channels, 3);
}

TEST(BlackSourceTest, CreateWithDifferentChannels) {
  // Test with 1 channel (grayscale)
  {
    ImageInfo dims(50, 50, 1, PixelType::kUInt8, DataLayout::kChannelsLast);
    auto source = BlackSource::Create(dims);
    EXPECT_EQ(source.GetChannels(), 1);
  }

  // Test with 3 channels (RGB)
  {
    ImageInfo dims(50, 50, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
    auto source = BlackSource::Create(dims);
    EXPECT_EQ(source.GetChannels(), 3);
  }

  // Test with 4 channels (RGBA)
  {
    ImageInfo dims(50, 50, 4, PixelType::kUInt8, DataLayout::kChannelsLast);
    auto source = BlackSource::Create(dims);
    EXPECT_EQ(source.GetChannels(), 4);
  }
}

TEST(BlackSourceTest, InvalidDimensions) {
  // Negative width
  EXPECT_THROW(
      {
        ImageInfo dims(-10, 100, 3, PixelType::kUInt8,
                       DataLayout::kChannelsLast);
        BlackSource::Create(dims);
      },
      std::invalid_argument);

  // Negative height
  EXPECT_THROW(
      {
        ImageInfo dims(100, -10, 3, PixelType::kUInt8,
                       DataLayout::kChannelsLast);
        BlackSource::Create(dims);
      },
      std::invalid_argument);

  // Zero width
  EXPECT_THROW(
      {
        ImageInfo dims(0, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
        BlackSource::Create(dims);
      },
      std::invalid_argument);

  // Zero channels
  EXPECT_THROW(
      {
        ImageInfo dims(100, 100, 0, PixelType::kUInt8,
                       DataLayout::kChannelsLast);
        BlackSource::Create(dims);
      },
      std::invalid_argument);
}

TEST(BlackSourceTest, GetTileReturnsBlackPixels) {
  ImageInfo dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto source = BlackSource::Create(dims);

  // Get a tile
  auto tile = source.GetTile(0, 0, 10, 10);

  // Check tile dimensions
  EXPECT_EQ(tile.width, 10);
  EXPECT_EQ(tile.height, 10);
  EXPECT_EQ(tile.channels, 3);
  EXPECT_EQ(tile.x, 0);
  EXPECT_EQ(tile.y, 0);

  // Check all pixels are black (zero)
  for (size_t i = 0; i < tile.GetData().size(); ++i) {
    EXPECT_EQ(tile.GetData()[i], 0)
        << "Pixel at index " << i << " is not black";
  }
}

TEST(BlackSourceTest, GetTileAtDifferentPositions) {
  ImageInfo dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto source = BlackSource::Create(dims);

  // Get tile at (50, 50)
  auto tile = source.GetTile(50, 50, 20, 20);

  EXPECT_EQ(tile.x, 50);
  EXPECT_EQ(tile.y, 50);
  EXPECT_EQ(tile.width, 20);
  EXPECT_EQ(tile.height, 20);

  // All pixels should still be black
  for (size_t i = 0; i < tile.GetData().size(); ++i) {
    EXPECT_EQ(tile.GetData()[i], 0);
  }
}

TEST(BlackSourceTest, GetTileClampedToBounds) {
  ImageInfo dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto source = BlackSource::Create(dims);

  // Request tile that extends beyond image bounds
  auto tile = source.GetTile(90, 90, 20, 20);

  // Should be clamped to 10x10
  EXPECT_EQ(tile.width, 10);
  EXPECT_EQ(tile.height, 10);
  EXPECT_EQ(tile.x, 90);
  EXPECT_EQ(tile.y, 90);

  // All pixels should be black
  for (size_t i = 0; i < tile.GetData().size(); ++i) {
    EXPECT_EQ(tile.GetData()[i], 0);
  }
}

TEST(BlackSourceTest, GetTileCompletelyOutOfBounds) {
  ImageInfo dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto source = BlackSource::Create(dims);

  // Request tile completely out of bounds
  auto tile = source.GetTile(200, 200, 10, 10);

  // Should return empty tile
  EXPECT_EQ(tile.width, 0);
  EXPECT_EQ(tile.height, 0);
}

TEST(BlackSourceTest, MemoryLayout) {
  ImageInfo dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto source = BlackSource::Create(dims);

  EXPECT_EQ(source.GetMemoryLayout(), DataLayout::kChannelsLast);
}

TEST(BlackSourceTest, IdealTileSize) {
  ImageInfo dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  TileSize custom_tile_size(512, 512);
  auto source = BlackSource::Create(dims, custom_tile_size);

  auto tile_size = source.GetIdealTileSize();
  EXPECT_EQ(tile_size.width, 512);
  EXPECT_EQ(tile_size.height, 512);
}

TEST(BlackSourceTest, LargeCanvas) {
  // Test with a large canvas to ensure no memory issues
  ImageInfo dims(10000, 10000, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto source = BlackSource::Create(dims);

  EXPECT_EQ(source.GetDimensions().GetWidth(), 10000);
  EXPECT_EQ(source.GetDimensions().GetHeight(), 10000);

  // Get a small tile from the large canvas
  auto tile = source.GetTile(5000, 5000, 100, 100);
  EXPECT_EQ(tile.width, 100);
  EXPECT_EQ(tile.height, 100);

  // Verify all pixels are black
  for (size_t i = 0; i < tile.GetData().size(); ++i) {
    EXPECT_EQ(tile.GetData()[i], 0);
  }
}

}  // namespace
}  // namespace fim

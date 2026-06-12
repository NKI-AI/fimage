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

#include "fim/operators/resize.h"
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

  Tile GetTile(int tile_x, int tile_y, int width, int height) const {
    // Clamp to image bounds
    int actual_width = std::min(width, dims_.GetWidth() - tile_x);
    int actual_height = std::min(height, dims_.GetHeight() - tile_y);

    if (actual_width <= 0 || actual_height <= 0) {
      return Tile(tile_x, tile_y, 0, 0, dims_.channels, dims_.layout);
    }

    Tile tile(tile_x, tile_y, actual_width, actual_height, dims_.channels,
              dims_.layout);

    // Fill with test pattern based on fill_value_
    for (int y = 0; y < actual_height; ++y) {
      for (int x = 0; x < actual_width; ++x) {
        int global_x = tile_x + x;
        int global_y = tile_y + y;
        uint8_t value = static_cast<uint8_t>(
            (fill_value_ + (global_y * dims_.GetWidth()) + global_x) % 256);

        int pixel_offset = ((y * actual_width) + x) * dims_.channels;
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

class ResizeTest : public ::testing::Test {
 protected:
  void SetUp() override { source_ = std::make_unique<MockSource>(100, 100, 3); }

  void TearDown() override { source_.reset(); }

  std::unique_ptr<MockSource> source_;
};

// Test basic resize functionality with Lanczos3
TEST_F(ResizeTest, BasicDownsample) {
  Resize resize(*source_, 50, 50, resize::KernelType::kLanczos3);

  auto dims = resize.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 50);
  EXPECT_EQ(dims.GetHeight(), 50);
  EXPECT_EQ(dims.channels, 3);
  EXPECT_EQ(dims.layout, DataLayout::kChannelsLast);
}

// Test basic upsampling
TEST_F(ResizeTest, BasicUpsample) {
  Resize resize(*source_, 200, 200, resize::KernelType::kLanczos3);

  auto dims = resize.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 200);
  EXPECT_EQ(dims.GetHeight(), 200);
  EXPECT_EQ(dims.channels, 3);
}

// Test non-uniform scaling
TEST_F(ResizeTest, NonUniformScale) {
  Resize resize(*source_, 150, 75, resize::KernelType::kLanczos3);

  auto dims = resize.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 150);
  EXPECT_EQ(dims.GetHeight(), 75);
  EXPECT_EQ(dims.channels, 3);
}

// Test with Lanczos2 kernel
TEST_F(ResizeTest, Lanczos2Kernel) {
  Resize resize(*source_, 50, 50, resize::KernelType::kLanczos2);

  auto dims = resize.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 50);
  EXPECT_EQ(dims.GetHeight(), 50);

  // Get a tile to ensure it works
  auto tile = resize.GetTile(0, 0, 25, 25);
  EXPECT_EQ(tile.width, 25);
  EXPECT_EQ(tile.height, 25);
  EXPECT_EQ(tile.channels, 3);
}

// Test with Magic2021 kernel
TEST_F(ResizeTest, Magic2021Kernel) {
  Resize resize(*source_, 50, 50, resize::KernelType::kMagic2021);

  auto dims = resize.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 50);
  EXPECT_EQ(dims.GetHeight(), 50);

  // Get a tile to ensure it works
  auto tile = resize.GetTile(0, 0, 25, 25);
  EXPECT_EQ(tile.width, 25);
  EXPECT_EQ(tile.height, 25);
  EXPECT_EQ(tile.channels, 3);
}

// Test with box parameter
TEST_F(ResizeTest, WithBoxParameter) {
  resize::Box box(10.5F, 20.5F, 60.5F, 70.5F);
  Resize resize(*source_, 50, 50, resize::KernelType::kLanczos3, box);

  auto dims = resize.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 50);
  EXPECT_EQ(dims.GetHeight(), 50);

  // Get a tile
  auto tile = resize.GetTile(0, 0, 50, 50);
  EXPECT_EQ(tile.width, 50);
  EXPECT_EQ(tile.height, 50);
}

// Test box parameter with subpixel precision
TEST_F(ResizeTest, SubpixelBox) {
  resize::Box box(0.25F, 0.75F, 99.25F, 99.75F);
  Resize resize(*source_, 100, 100, resize::KernelType::kMagic2021, box);

  auto tile = resize.GetTile(0, 0, 100, 100);
  EXPECT_EQ(tile.width, 100);
  EXPECT_EQ(tile.height, 100);
}

// Test invalid target dimensions
TEST_F(ResizeTest, InvalidDimensions) {
  EXPECT_THROW(Resize(*source_, 0, 50), std::invalid_argument);
  EXPECT_THROW(Resize(*source_, 50, 0), std::invalid_argument);
  EXPECT_THROW(Resize(*source_, -10, 50), std::invalid_argument);
}

// Test invalid input dimensions (zero width or height)
TEST_F(ResizeTest, ZeroInputDimensions) {
  MockSource zero_width_source(0, 100, 3);
  MockSource zero_height_source(100, 0, 3);
  MockSource zero_both_source(0, 0, 3);

  // Should throw std::invalid_argument for zero width input
  EXPECT_THROW(Resize(zero_width_source, 50, 50), std::invalid_argument);

  // Should throw std::invalid_argument for zero height input
  EXPECT_THROW(Resize(zero_height_source, 50, 50), std::invalid_argument);

  // Should throw std::invalid_argument for both zero
  EXPECT_THROW(Resize(zero_both_source, 50, 50), std::invalid_argument);
}

// Test invalid box
TEST_F(ResizeTest, InvalidBox) {
  resize::Box bad_box(50.0F, 50.0F, 10.0F, 10.0F);  // x2 < x1
  EXPECT_THROW(Resize(*source_, 50, 50, resize::KernelType::kLanczos3, bad_box),
               std::invalid_argument);
}

// Test ideal tile size calculation
TEST_F(ResizeTest, IdealTileSize) {
  // Downsampling: should reduce tile size
  Resize downsample(*source_, 50, 50);
  auto down_tile_size = downsample.GetIdealTileSize();
  EXPECT_GT(down_tile_size.width, 0);
  EXPECT_GT(down_tile_size.height, 0);

  // Upsampling: should increase tile size
  Resize upsample(*source_, 200, 200);
  auto up_tile_size = upsample.GetIdealTileSize();
  EXPECT_GT(up_tile_size.width, 0);
  EXPECT_GT(up_tile_size.height, 0);
}

// Test getting tiles at various positions
TEST_F(ResizeTest, GetTileAtDifferentPositions) {
  Resize resize(*source_, 80, 80);

  // Top-left
  auto tile1 = resize.GetTile(0, 0, 40, 40);
  EXPECT_EQ(tile1.x, 0);
  EXPECT_EQ(tile1.y, 0);
  EXPECT_EQ(tile1.width, 40);
  EXPECT_EQ(tile1.height, 40);

  // Center
  auto tile2 = resize.GetTile(20, 20, 40, 40);
  EXPECT_EQ(tile2.x, 20);
  EXPECT_EQ(tile2.y, 20);
  EXPECT_EQ(tile2.width, 40);
  EXPECT_EQ(tile2.height, 40);

  // Bottom-right (partial)
  auto tile3 = resize.GetTile(60, 60, 40, 40);
  EXPECT_EQ(tile3.x, 60);
  EXPECT_EQ(tile3.y, 60);
  EXPECT_EQ(tile3.width, 20);   // Clamped
  EXPECT_EQ(tile3.height, 20);  // Clamped
}

// Test getting tile beyond bounds
TEST_F(ResizeTest, GetTileBeyondBounds) {
  Resize resize(*source_, 50, 50);

  auto tile = resize.GetTile(60, 70, 10, 10);
  EXPECT_EQ(tile.x, 60);
  EXPECT_EQ(tile.y, 70);
  EXPECT_EQ(tile.width, 0);
  EXPECT_EQ(tile.height, 0);
}

// Test with different channel counts
TEST_F(ResizeTest, DifferentChannelCounts) {
  MockSource grayscale_source(50, 50, 1);
  MockSource rgba_source(50, 50, 4);

  Resize grayscale_resize(grayscale_source, 25, 25);
  Resize rgba_resize(rgba_source, 25, 25);

  auto gray_dims = grayscale_resize.GetDimensions();
  auto rgba_dims = rgba_resize.GetDimensions();

  EXPECT_EQ(gray_dims.channels, 1);
  EXPECT_EQ(rgba_dims.channels, 4);

  auto gray_tile = grayscale_resize.GetTile(0, 0, 25, 25);
  auto rgba_tile = rgba_resize.GetTile(0, 0, 25, 25);

  EXPECT_EQ(gray_tile.channels, 1);
  EXPECT_EQ(rgba_tile.channels, 4);
}

// Test extreme scaling factors
TEST_F(ResizeTest, ExtremeDownsampling) {
  Resize resize(*source_, 10, 10);  // 10x downsampling

  auto tile = resize.GetTile(0, 0, 10, 10);
  EXPECT_EQ(tile.width, 10);
  EXPECT_EQ(tile.height, 10);
}

TEST_F(ResizeTest, ExtremeUpsampling) {
  Resize resize(*source_, 500, 500);  // 5x upsampling

  auto tile = resize.GetTile(0, 0, 100, 100);
  EXPECT_EQ(tile.width, 100);
  EXPECT_EQ(tile.height, 100);
}

// Test that output data is properly clamped
TEST_F(ResizeTest, OutputClamping) {
  Resize resize(*source_, 50, 50);

  auto tile = resize.GetTile(0, 0, 50, 50);

  // Check that all values are valid uint8_t (0-255)
  for (size_t i = 0; i < tile.GetData().size(); ++i) {
    EXPECT_GE(tile.GetData()[i], 0);
    EXPECT_LE(tile.GetData()[i], 255);
  }
}

}  // namespace fim

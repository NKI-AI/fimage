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

#include "fim/sources/tiff_source.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

#include "fim/types.h"

namespace fim {

class TiffSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // We'll use the existing checkerboard.svs file for testing
    // as it's already in the project root
    test_file_ = "../../checkerboard.svs";
  }

  void TearDown() override {
    // No cleanup needed for existing test files
  }

  std::string test_file_;
};

// Test TIFF source construction and basic properties
TEST_F(TiffSourceTest, Construction) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test TIFF file not found: " << test_file_;
  }

  TiffSource source = TiffSource::Create(test_file_);

  auto dims = source.GetDimensions();
  EXPECT_GT(dims.GetWidth(), 0);
  EXPECT_GT(dims.GetHeight(), 0);
  EXPECT_GT(dims.channels, 0);
  EXPECT_LE(dims.channels, 4);
}

// Test TIFF source with non-existent file
TEST_F(TiffSourceTest, NonExistentFile) {
  EXPECT_THROW(TiffSource::Create("non_existent.tiff"), std::runtime_error);
}

// Test ideal tile size
TEST_F(TiffSourceTest, IdealTileSize) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test TIFF file not found: " << test_file_;
  }

  TiffSource source = TiffSource::Create(test_file_);

  auto tile_size = source.GetIdealTileSize();
  EXPECT_GT(tile_size.width, 0);
  EXPECT_GT(tile_size.height, 0);
}

// Test getting a tile from the TIFF
TEST_F(TiffSourceTest, GetTile) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test TIFF file not found: " << test_file_;
  }

  TiffSource source = TiffSource::Create(test_file_);
  auto dims = source.GetDimensions();

  // Get a small tile from the top-left corner
  int tile_width = std::min(100, dims.GetWidth());
  int tile_height = std::min(100, dims.GetHeight());

  auto tile = source.GetTile(0, 0, tile_width, tile_height);
  EXPECT_EQ(tile.x, 0);
  EXPECT_EQ(tile.y, 0);
  EXPECT_EQ(tile.width, tile_width);
  EXPECT_EQ(tile.height, tile_height);
  EXPECT_EQ(tile.channels, dims.channels);
  EXPECT_EQ(tile.GetData().size(), tile_width * tile_height * dims.channels);
}

// Test getting a tile beyond image bounds
TEST_F(TiffSourceTest, GetTileBeyondBounds) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test TIFF file not found: " << test_file_;
  }

  TiffSource source = TiffSource::Create(test_file_);
  auto dims = source.GetDimensions();

  // Try to get a tile that extends beyond the image
  int start_x = dims.GetWidth() - 50;
  int start_y = dims.GetHeight() - 50;

  auto tile = source.GetTile(start_x, start_y, 100, 100);
  EXPECT_EQ(tile.x, start_x);
  EXPECT_EQ(tile.y, start_y);
  EXPECT_EQ(tile.width, 50);   // Should be clamped
  EXPECT_EQ(tile.height, 50);  // Should be clamped
  EXPECT_EQ(tile.channels, dims.channels);
}

// Test getting a tile completely outside image bounds
TEST_F(TiffSourceTest, GetTileOutsideBounds) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test TIFF file not found: " << test_file_;
  }

  TiffSource source = TiffSource::Create(test_file_);
  auto dims = source.GetDimensions();

  auto tile =
      source.GetTile(dims.GetWidth() + 100, dims.GetHeight() + 100, 50, 50);
  EXPECT_EQ(tile.x, dims.GetWidth() + 100);
  EXPECT_EQ(tile.y, dims.GetHeight() + 100);
  EXPECT_EQ(tile.width, 0);
  EXPECT_EQ(tile.height, 0);
  EXPECT_EQ(tile.channels, dims.channels);
  EXPECT_TRUE(tile.GetData().empty());
}

// Test move constructor
TEST_F(TiffSourceTest, MoveConstructor) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test TIFF file not found: " << test_file_;
  }

  TiffSource source1 = TiffSource::Create(test_file_);
  auto dims1 = source1.GetDimensions();

  TiffSource source2(std::move(source1));
  auto dims2 = source2.GetDimensions();

  EXPECT_EQ(dims2.GetWidth(), dims1.GetWidth());
  EXPECT_EQ(dims2.GetHeight(), dims1.GetHeight());
  EXPECT_EQ(dims2.channels, dims1.channels);
}

// Test move assignment
TEST_F(TiffSourceTest, MoveAssignment) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test TIFF file not found: " << test_file_;
  }

  TiffSource source1 = TiffSource::Create(test_file_);
  TiffSource source2 = TiffSource::Create(test_file_);

  auto dims1 = source1.GetDimensions();
  source2 = std::move(source1);
  auto dims2 = source2.GetDimensions();

  EXPECT_EQ(dims2.GetWidth(), dims1.GetWidth());
  EXPECT_EQ(dims2.GetHeight(), dims1.GetHeight());
  EXPECT_EQ(dims2.channels, dims1.channels);
}

}  // namespace fim

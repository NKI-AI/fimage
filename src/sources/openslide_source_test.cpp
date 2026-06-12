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

#include "fim/sources/openslide_source.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "fim/image.h"
#include "fim/operators/crop.h"
#include "fim/operators/downsample.h"
#include "fim/types.h"

namespace fim {

class OpenSlideSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use existing test SVS files
    test_file_ = "../../CMU-1.tiff";
    test_file_svs_ = "../../CMU-2.svs";
  }

  std::string test_file_;
  std::string test_file_svs_;
};

// Test OpenSlideSource construction and basic properties
TEST_F(OpenSlideSourceTest, Construction) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);

  EXPECT_GT(source.GetLevelCount(), 0);
  EXPECT_NO_THROW(source.GetMpp());
  EXPECT_FALSE(source.GetFormatName().empty());
}

// Test OpenSlideSource with non-existent file
TEST_F(OpenSlideSourceTest, NonExistentFile) {
  EXPECT_THROW(OpenSlideSource::Create("non_existent.svs"), std::runtime_error);
}

// Test level count and metadata
TEST_F(OpenSlideSourceTest, LevelMetadata) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);
  int level_count = source.GetLevelCount();

  EXPECT_GT(level_count, 0);

  // Test channel count (OpenSlide always returns 3 for RGB)
  int channels = source.GetNumChannels();
  EXPECT_EQ(channels, 3) << "OpenSlide should always return 3 channels (RGB)";

  // Test each level
  for (int level = 0; level < level_count; ++level) {
    auto dims = source.GetLevelDimensions(level);
    EXPECT_GT(dims.width, 0) << "Level " << level << " has invalid width";
    EXPECT_GT(dims.height, 0) << "Level " << level << " has invalid height";

    double downsample = source.GetLevelDownsample(level);
    EXPECT_GE(downsample, 1.0)
        << "Level " << level << " has invalid downsample";

    // Level 0 should have downsample = 1.0
    if (level == 0) {
      EXPECT_DOUBLE_EQ(downsample, 1.0);
    }
  }
}

// Test invalid level access
TEST_F(OpenSlideSourceTest, InvalidLevel) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);
  int level_count = source.GetLevelCount();

  // Test out of range levels
  EXPECT_THROW(source.GetLevelDimensions(-1), std::out_of_range);
  EXPECT_THROW(source.GetLevelDimensions(level_count), std::out_of_range);
  EXPECT_THROW(source.GetLevelDownsample(-1), std::out_of_range);
  EXPECT_THROW(source.GetLevelDownsample(level_count), std::out_of_range);
  EXPECT_THROW(source.LevelView(-1), std::out_of_range);
  EXPECT_THROW(source.LevelView(level_count), std::out_of_range);
}

// Test MPP (microns per pixel)
TEST_F(OpenSlideSourceTest, MicronsPerPixel) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);
  auto mpp = source.GetMpp();

  // MPP should be positive (or 0 if not calibrated)
  EXPECT_GE(mpp[0], 0.0);
  EXPECT_GE(mpp[1], 0.0);
}

// Test LevelView creation and basic properties
TEST_F(OpenSlideSourceTest, LevelViewCreation) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);
  int level_count = source.GetLevelCount();

  for (int level = 0; level < level_count; ++level) {
    auto level_view = source.LevelView(level);

    auto dims = level_view.GetDimensions();
    auto expected_dims = source.GetLevelDimensions(level);
    int expected_channels = source.GetNumChannels();

    EXPECT_EQ(dims.GetWidth(), expected_dims.width);
    EXPECT_EQ(dims.GetHeight(), expected_dims.height);
    EXPECT_EQ(dims.channels, expected_channels);

    auto tile_size = level_view.GetIdealTileSize();
    EXPECT_GT(tile_size.width, 0);
    EXPECT_GT(tile_size.height, 0);
  }
}

// Test getting a tile from a level view
TEST_F(OpenSlideSourceTest, GetTileFromLevelView) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);
  auto level0_view = source.LevelView(0);
  auto dims = level0_view.GetDimensions();

  // Get a small tile from the top-left corner
  int tile_width = std::min(256, dims.GetWidth());
  int tile_height = std::min(256, dims.GetHeight());

  auto tile = level0_view.GetTile(0, 0, tile_width, tile_height);

  EXPECT_EQ(tile.x, 0);
  EXPECT_EQ(tile.y, 0);
  EXPECT_EQ(tile.width, tile_width);
  EXPECT_EQ(tile.height, tile_height);
  EXPECT_EQ(tile.channels, dims.channels);
  EXPECT_EQ(tile.GetData().size(), tile_width * tile_height * dims.channels);
}

// Test level switching - coordinates are independent per level
TEST_F(OpenSlideSourceTest, LevelSwitching) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);
  if (source.GetLevelCount() < 2) {
    GTEST_SKIP() << "Test requires at least 2 pyramid levels";
  }

  // Get tiles from different levels at the same coordinates
  auto level0_view = source.LevelView(0);
  auto level1_view = source.LevelView(1);

  int tile_size = 128;

  // These are in different coordinate systems
  auto tile0 = level0_view.GetTile(100, 100, tile_size, tile_size);
  auto tile1 = level1_view.GetTile(100, 100, tile_size, tile_size);

  // Both should be valid but represent different physical regions
  EXPECT_EQ(tile0.width, tile_size);
  EXPECT_EQ(tile1.width, tile_size);
  EXPECT_EQ(tile0.height, tile_size);
  EXPECT_EQ(tile1.height, tile_size);
}

// Test integration with Crop operator
TEST_F(OpenSlideSourceTest, IntegrationWithCrop) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);
  auto level_view = source.LevelView(0);
  auto dims = level_view.GetDimensions();

  int crop_size = std::min(512, std::min(dims.GetWidth(), dims.GetHeight()));

  // Create a crop operator
  Crop<OpenSlideLevelView> crop_op(std::move(level_view), 0, 0, crop_size,
                                   crop_size);

  auto crop_dims = crop_op.GetDimensions();
  EXPECT_EQ(crop_dims.GetWidth(), crop_size);
  EXPECT_EQ(crop_dims.GetHeight(), crop_size);

  // Get a tile from the cropped view
  auto tile = crop_op.GetTile(0, 0, crop_size, crop_size);
  EXPECT_EQ(tile.width, crop_size);
  EXPECT_EQ(tile.height, crop_size);
}

// Test integration with Downsample operator
TEST_F(OpenSlideSourceTest, IntegrationWithDownsample) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);
  auto level_view = source.LevelView(0);
  auto dims = level_view.GetDimensions();

  int downsample_factor = 2;

  // Create a downsample operator
  Downsample<OpenSlideLevelView> ds_op(std::move(level_view),
                                       downsample_factor);

  auto ds_dims = ds_op.GetDimensions();
  EXPECT_EQ(ds_dims.GetWidth(), dims.GetWidth() / downsample_factor);
  EXPECT_EQ(ds_dims.GetHeight(), dims.GetHeight() / downsample_factor);
  EXPECT_EQ(ds_dims.channels, dims.channels);

  // Get a tile from the downsampled view
  int tile_size = 64;
  auto tile = ds_op.GetTile(0, 0, tile_size, tile_size);
  EXPECT_EQ(tile.width, tile_size);
  EXPECT_EQ(tile.height, tile_size);
}

// Test move semantics
TEST_F(OpenSlideSourceTest, MoveSemantics) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source1 = OpenSlideSource::Create(test_file_);
  int level_count = source1.GetLevelCount();

  // Move construct
  auto source2 = std::move(source1);
  EXPECT_EQ(source2.GetLevelCount(), level_count);

  // Move assign
  auto source3 = OpenSlideSource::Create(test_file_);
  source3 = std::move(source2);
  EXPECT_EQ(source3.GetLevelCount(), level_count);
}

// Test with multiple slides
TEST_F(OpenSlideSourceTest, MultipleSlides) {
  if (!std::filesystem::exists(test_file_) ||
      !std::filesystem::exists(test_file_svs_)) {
    GTEST_SKIP() << "Test files not found";
  }

  auto source1 = OpenSlideSource::Create(test_file_);
  auto source2 = OpenSlideSource::Create(test_file_svs_);

  // Both should be valid
  EXPECT_GT(source1.GetLevelCount(), 0);
  EXPECT_GT(source2.GetLevelCount(), 0);

  // They should have different dimensions
  auto dims1 = source1.GetLevelDimensions(0);
  auto dims2 = source2.GetLevelDimensions(0);

  // At least one dimension should differ (likely both)
  bool different =
      (dims1.width != dims2.width) || (dims1.height != dims2.height);
  EXPECT_TRUE(different);
}

// Test memory format
TEST_F(OpenSlideSourceTest, MemoryFormat) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);

  // OpenSlide should always return channels-last (RGB) format
  EXPECT_EQ(source.GetMemoryFormat(), DataLayout::kChannelsLast);
}

// Test that LevelView can outlive the source (lifetime safety)
TEST_F(OpenSlideSourceTest, LevelViewOutlivesSource) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  // Create a level view from a temporary source
  OpenSlideLevelView view = [this]() {
    auto source = OpenSlideSource::Create(test_file_);
    return source.LevelView(0);
  }();  // source is destroyed here, but view should keep handle alive

  // View should still be usable after source is destroyed
  auto dims = view.GetDimensions();
  EXPECT_GT(dims.GetWidth(), 0);
  EXPECT_GT(dims.GetHeight(), 0);

  // Should be able to get tiles
  int tile_size = std::min(128, std::min(dims.GetWidth(), dims.GetHeight()));
  auto tile = view.GetTile(0, 0, tile_size, tile_size);
  EXPECT_EQ(tile.width, tile_size);
  EXPECT_EQ(tile.height, tile_size);
}

// Test creating view directly from temporary source
TEST_F(OpenSlideSourceTest, TemporarySourceLevelView) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  // This pattern used to be unsafe, but is now safe with shared ownership
  auto view = OpenSlideSource::Create(test_file_).LevelView(0);

  auto dims = view.GetDimensions();
  EXPECT_GT(dims.GetWidth(), 0);
  EXPECT_GT(dims.GetHeight(), 0);

  int tile_size = 64;
  auto tile = view.GetTile(0, 0, tile_size, tile_size);
  EXPECT_EQ(tile.width, tile_size);
  EXPECT_EQ(tile.height, tile_size);
}

// Test multiple views sharing the same underlying handle and mutex
TEST_F(OpenSlideSourceTest, MultipleViewsShareHandle) {
  if (!std::filesystem::exists(test_file_)) {
    GTEST_SKIP() << "Test file not found: " << test_file_;
  }

  auto source = OpenSlideSource::Create(test_file_);
  if (source.GetLevelCount() < 2) {
    GTEST_SKIP() << "Test requires at least 2 pyramid levels";
  }

  // Create multiple views
  auto view0 = source.LevelView(0);
  auto view1 = source.LevelView(1);

  // Destroy the source - views should keep handle/mutex alive via shared
  // ownership
  { auto temp_source = std::move(source); }  // temp_source destroyed here

  // Both views should still work (thread-safe with shared mutex)
  auto dims0 = view0.GetDimensions();
  auto dims1 = view1.GetDimensions();
  EXPECT_GT(dims0.GetWidth(), 0);
  EXPECT_GT(dims1.GetWidth(), 0);

  // Should be able to get tiles from both views
  int tile_size = 64;
  auto tile0 = view0.GetTile(0, 0, tile_size, tile_size);
  auto tile1 = view1.GetTile(0, 0, tile_size, tile_size);
  EXPECT_EQ(tile0.width, tile_size);
  EXPECT_EQ(tile1.width, tile_size);
}

}  // namespace fim

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

#include <fim/sinks/tiff_sink.h>
#include <fim/types.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

namespace fim {

// Mock source for testing sinks
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

    // Fill with test pattern: gradient
    for (int tile_y = 0; tile_y < actual_height; ++tile_y) {
      for (int tile_x = 0; tile_x < actual_width; ++tile_x) {
        int global_x = x + tile_x;
        int global_y = y + tile_y;
        uint8_t value = static_cast<uint8_t>((global_x + global_y) % 256);

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

class TiffSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {
    // Clean up test files
    std::filesystem::remove("test_output.tiff");
    std::filesystem::remove("test_tiled.tiff");
    std::filesystem::remove("test_strip.tiff");
    std::filesystem::remove("test_single.tiff");
    std::filesystem::remove("test_pyramidal.tiff");
  }
};

// Test TIFF sink with default tile size
TEST_F(TiffSinkTest, DefaultTileSize) {
  MockSource source(100, 80, 3);
  TiffSink sink = TiffSink::Create("test_output.tiff");

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test TIFF sink with custom tile size
TEST_F(TiffSinkTest, CustomTileSize) {
  MockSource source(200, 150, 3);
  TiffSink sink = TiffSink::Create("test_tiled.tiff", TileSize(64, 64));

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_tiled.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_tiled.tiff"), 0);
}

// Test TIFF sink with strip format (large tile size)
TEST_F(TiffSinkTest, StripFormat) {
  MockSource source(100, 80, 3);
  TiffSink sink = TiffSink::Create("test_strip.tiff",
                                   TileSize(1000, 1000));  // Larger than image

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_strip.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_strip.tiff"), 0);
}

// Test TIFF sink with different channel counts
TEST_F(TiffSinkTest, DifferentChannels) {
  MockSource grayscale_source(50, 40, 1);
  MockSource rgba_source(60, 50, 4);

  TiffSink grayscale_sink = TiffSink::Create("test_output.tiff");
  TiffSink rgba_sink = TiffSink::Create("test_output.tiff");

  grayscale_sink.Render(grayscale_source);
  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));

  std::filesystem::remove("test_output.tiff");

  rgba_sink.Render(rgba_source);
  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
}

// Test TIFF sink with small image
TEST_F(TiffSinkTest, SmallImage) {
  MockSource source(10, 8, 3);
  TiffSink sink = TiffSink::Create("test_output.tiff");

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test TIFF sink with large image
TEST_F(TiffSinkTest, LargeImage) {
  MockSource source(1000, 800, 3);
  TiffSink sink = TiffSink::Create("test_output.tiff", TileSize(512, 512));

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test pyramidal TIFF creation with small image
TEST_F(TiffSinkTest, PyramidalSmallImage) {
  MockSource source(64, 64, 3);
  TiffSink sink =
      TiffSink::Create("test_output.tiff", TileSize(32, 32), true, 1,
                       2);  // pyramidal, 1MB threshold

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test pyramidal TIFF creation with medium image
TEST_F(TiffSinkTest, PyramidalMediumImage) {
  MockSource source(256, 256, 3);
  TiffSink sink =
      TiffSink::Create("test_output.tiff", TileSize(64, 64), true, 50,
                       2);  // pyramidal, 50MB threshold

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test pyramidal TIFF creation with large image and memory optimization
TEST_F(TiffSinkTest, PyramidalLargeImageWithMemoryOptimization) {
  MockSource source(512, 512, 3);
  TiffSink sink =
      TiffSink::Create("test_output.tiff", TileSize(128, 128), true, 10,
                       2);  // pyramidal, 10MB threshold

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test pyramidal TIFF with different downsample factors
TEST_F(TiffSinkTest, PyramidalDifferentDownsampleFactors) {
  MockSource source(128, 128, 3);

  // Test with downsample factor 4
  TiffSink sink4 =
      TiffSink::Create("test_output.tiff", TileSize(32, 32), true, 50, 4);
  sink4.Render(source);
  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));

  std::filesystem::remove("test_output.tiff");

  // Test with downsample factor 3
  TiffSink sink3 =
      TiffSink::Create("test_output.tiff", TileSize(32, 32), true, 50, 3);
  sink3.Render(source);
  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
}

// Test pyramidal TIFF with different channel counts
TEST_F(TiffSinkTest, PyramidalDifferentChannels) {
  MockSource grayscale_source(128, 128, 1);
  MockSource rgba_source(128, 128, 4);

  TiffSink grayscale_sink =
      TiffSink::Create("test_output.tiff", TileSize(64, 64), true, 50, 2);
  grayscale_sink.Render(grayscale_source);
  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));

  std::filesystem::remove("test_output.tiff");

  TiffSink rgba_sink =
      TiffSink::Create("test_output.tiff", TileSize(64, 64), true, 50, 2);
  rgba_sink.Render(rgba_source);
  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
}

// Test pyramidal TIFF with very small memory threshold (forces source-based
// processing)
TEST_F(TiffSinkTest, PyramidalVerySmallMemoryThreshold) {
  MockSource source(128, 128, 3);
  TiffSink sink =
      TiffSink::Create("test_output.tiff", TileSize(32, 32), true, 1,
                       2);  // 1MB threshold - very small

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test pyramidal TIFF with very large memory threshold (forces memory-based
// processing)
TEST_F(TiffSinkTest, PyramidalVeryLargeMemoryThreshold) {
  MockSource source(128, 128, 3);
  TiffSink sink =
      TiffSink::Create("test_output.tiff", TileSize(32, 32), true, 1000,
                       2);  // 1GB threshold - very large

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test pyramidal vs single-page TIFF file size difference
TEST_F(TiffSinkTest, PyramidalVsSinglePageFileSize) {
  MockSource source(200, 200, 3);

  // Create single-page TIFF
  TiffSink single_sink =
      TiffSink::Create("test_single.tiff", TileSize(64, 64), false);
  single_sink.Render(source);

  // Create pyramidal TIFF
  TiffSink pyramidal_sink =
      TiffSink::Create("test_pyramidal.tiff", TileSize(64, 64), true, 50, 2);
  pyramidal_sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_single.tiff"));
  EXPECT_TRUE(std::filesystem::exists("test_pyramidal.tiff"));

  auto single_size = std::filesystem::file_size("test_single.tiff");
  auto pyramidal_size = std::filesystem::file_size("test_pyramidal.tiff");

  // Pyramidal should be larger due to multiple levels
  EXPECT_GT(pyramidal_size, single_size);

  // Clean up extra files
  std::filesystem::remove("test_single.tiff");
  std::filesystem::remove("test_pyramidal.tiff");
}

// Test edge case: image smaller than tile size
TEST_F(TiffSinkTest, PyramidalImageSmallerThanTileSize) {
  MockSource source(16, 16, 3);
  TiffSink sink =
      TiffSink::Create("test_output.tiff", TileSize(32, 32), true, 50, 2);

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test edge case: single pixel image
TEST_F(TiffSinkTest, PyramidalSinglePixelImage) {
  MockSource source(1, 1, 3);
  TiffSink sink =
      TiffSink::Create("test_output.tiff", TileSize(32, 32), true, 50, 2);

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

}  // namespace fim

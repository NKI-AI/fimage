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

#include "fim/sinks/spng_sink.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "fim/types.h"

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

    // Fill with test pattern: alternating stripes
    for (int tile_y = 0; tile_y < actual_height; ++tile_y) {
      for (int tile_x = 0; tile_x < actual_width; ++tile_x) {
        int global_x = x + tile_x;
        int global_y = y + tile_y;
        uint8_t value =
            static_cast<uint8_t>((global_x + global_y) % 2 == 0 ? 255 : 0);

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

class SpngSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {
    // Clean up test files
    std::filesystem::remove("test_output.png");
    std::filesystem::remove("spng_sink_test_rgb.png");
    std::filesystem::remove("spng_sink_test_rgba.png");
    std::filesystem::remove("spng_sink_test_grayscale.png");
  }
};

// Test PNG sink with RGB source
TEST_F(SpngSinkTest, RgbOutput) {
  MockSource source(50, 40, 3);
  SpngSink sink = SpngSink::Create("spng_sink_test_rgb.png");

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("spng_sink_test_rgb.png"));
  EXPECT_GT(std::filesystem::file_size("spng_sink_test_rgb.png"), 0);
}

// Test PNG sink with RGBA source
TEST_F(SpngSinkTest, RgbaOutput) {
  MockSource source(60, 50, 4);
  SpngSink sink = SpngSink::Create("spng_sink_test_rgba.png");

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("spng_sink_test_rgba.png"));
  EXPECT_GT(std::filesystem::file_size("spng_sink_test_rgba.png"), 0);
}

// Test PNG sink with grayscale source
TEST_F(SpngSinkTest, GrayscaleOutput) {
  MockSource source(30, 25, 1);
  SpngSink sink = SpngSink::Create("spng_sink_test_grayscale.png");

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("spng_sink_test_grayscale.png"));
  EXPECT_GT(std::filesystem::file_size("spng_sink_test_grayscale.png"), 0);
}

// Test PNG sink with unsupported channel count
TEST_F(SpngSinkTest, UnsupportedChannels) {
  MockSource source(50, 40, 2);  // 2 channels not supported
  SpngSink sink = SpngSink::Create("test_output.png");

  EXPECT_THROW(sink.Render(source), std::runtime_error);
}

// Test PNG sink with invalid dimensions
TEST_F(SpngSinkTest, InvalidDimensions) {
  MockSource source(0, 0, 3);  // Invalid dimensions
  SpngSink sink = SpngSink::Create("test_output.png");

  EXPECT_THROW(sink.Render(source), std::runtime_error);
}

// Test PNG sink with large image (verifies streaming works efficiently)
TEST_F(SpngSinkTest, LargeImage) {
  MockSource source(1000, 800, 3);
  SpngSink sink = SpngSink::Create("test_output.png");

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.png"));
  EXPECT_GT(std::filesystem::file_size("test_output.png"), 0);
}

// Test PNG sink with small image
TEST_F(SpngSinkTest, SmallImage) {
  MockSource source(1, 1, 3);
  SpngSink sink = SpngSink::Create("test_output.png");

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.png"));
  EXPECT_GT(std::filesystem::file_size("test_output.png"), 0);
}

// Test PNG sink with different tile sizes (non-power-of-2 dimensions)
TEST_F(SpngSinkTest, DifferentTileSizes) {
  MockSource source(123, 87, 3);  // Non-power-of-2 dimensions
  SpngSink sink = SpngSink::Create("test_output.png");

  sink.Render(source);

  EXPECT_TRUE(std::filesystem::exists("test_output.png"));
  EXPECT_GT(std::filesystem::file_size("test_output.png"), 0);
}

}  // namespace fim

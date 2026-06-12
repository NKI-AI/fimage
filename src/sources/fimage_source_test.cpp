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
 * @file fimage_source_test.cpp
 * @brief Tests for FImage native format source.
 * @author Jonas Teuwen
 * @date 2025
 */
#include "fim/sources/fimage_source.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "fim/sinks/fimage_sink.h"
#include "fim/sources/memory_source.h"

namespace fim {
namespace {

// Helper to create test image data with a pattern
std::vector<uint8_t> CreatePatternImageData(int width, int height,
                                            int channels) {
  std::vector<uint8_t> data(width * height * channels);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      for (int c = 0; c < channels; ++c) {
        // Create a pattern that's easy to verify
        int idx = (y * width + x) * channels + c;
        data[idx] = static_cast<uint8_t>((x + y + c) % 256);
      }
    }
  }
  return data;
}

std::vector<uint8_t> CreatePatternImageDataCHW(int width, int height,
                                               int channels) {
  std::vector<uint8_t> data(width * height * channels);
  for (int c = 0; c < channels; ++c) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int idx = (c * height + y) * width + x;
        data[idx] = static_cast<uint8_t>((x + y + c) % 256);
      }
    }
  }
  return data;
}

// Helper to compare image data
bool CompareImageData(const std::vector<uint8_t>& a,
                      const std::vector<uint8_t>& b) {
  if (a.size() != b.size())
    return false;
  return std::equal(a.begin(), a.end(), b.begin());
}

// Test fixture
class FImageSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = fs::temp_directory_path() / "fimage_source_test";
    fs::create_directories(test_dir_);
  }

  void TearDown() override {
    if (fs::exists(test_dir_)) {
      fs::remove_all(test_dir_);
    }
  }

  fs::path GetTestPath(const std::string& name) const {
    return test_dir_ / name;
  }

  fs::path test_dir_;
};

TEST_F(FImageSourceTest, RoundTripContiguousUncompressed) {
  int width = 64;
  int height = 48;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write with sink
  auto path = GetTestPath("roundtrip_contiguous.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path);
  sink.Render(source);

  // Read with source
  auto fimage_source = FImageSource::Create(path);

  // Verify dimensions
  auto dims = fimage_source.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), width);
  EXPECT_EQ(dims.GetHeight(), height);
  EXPECT_EQ(dims.channels, channels);

  // Read full image
  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, RoundTripContiguousUncompressedChannelsFirst) {
  int width = 64;
  int height = 48;
  int channels = 3;
  auto original_data = CreatePatternImageDataCHW(width, height, channels);

  auto path = GetTestPath("roundtrip_contiguous_chw.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsFirst),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path);
  sink.Render(source);

  auto fimage_source = FImageSource::Create(path);
  auto dims = fimage_source.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), width);
  EXPECT_EQ(dims.GetHeight(), height);
  EXPECT_EQ(dims.channels, channels);
  EXPECT_EQ(dims.layout, DataLayout::kChannelsFirst);

  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_EQ(tile.layout, DataLayout::kChannelsFirst);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, RoundTripTiledUncompressed) {
  int width = 100;
  int height = 80;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write with sink (tiled)
  auto path = GetTestPath("roundtrip_tiled.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink =
      FImageSink::Create(path, TileSize(32, 32), CompressionType::kNone);
  sink.Render(source);

  // Read with source
  auto fimage_source = FImageSource::Create(path);

  // Verify ideal tile size matches what we wrote
  auto ideal_tile = fimage_source.GetIdealTileSize();
  EXPECT_EQ(ideal_tile.width, 32);
  EXPECT_EQ(ideal_tile.height, 32);

  // Read full image
  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, RoundTripTiledUncompressedChannelsFirst) {
  int width = 100;
  int height = 80;
  int channels = 3;
  auto original_data = CreatePatternImageDataCHW(width, height, channels);

  auto path = GetTestPath("roundtrip_tiled_chw.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsFirst),
                           TileSize(16, 16));
  auto sink =
      FImageSink::Create(path, TileSize(32, 32), CompressionType::kNone);
  sink.Render(source);

  auto fimage_source = FImageSource::Create(path);
  auto dims = fimage_source.GetDimensions();
  EXPECT_EQ(dims.layout, DataLayout::kChannelsFirst);

  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_EQ(tile.layout, DataLayout::kChannelsFirst);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, RoundTripContiguousLZ4) {
  int width = 64;
  int height = 48;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write with LZ4 compression
  auto path = GetTestPath("roundtrip_lz4.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path, TileSize(0, 0), CompressionType::kLZ4);
  sink.Render(source);

  // Read with source
  auto fimage_source = FImageSource::Create(path);

  // Read full image
  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, RoundTripContiguousLZ4ChannelsFirst) {
  int width = 64;
  int height = 48;
  int channels = 3;
  auto original_data = CreatePatternImageDataCHW(width, height, channels);

  auto path = GetTestPath("roundtrip_lz4_chw.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsFirst),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path, TileSize(0, 0), CompressionType::kLZ4);
  sink.Render(source);

  auto fimage_source = FImageSource::Create(path);
  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, RoundTripTiledLZ4) {
  int width = 100;
  int height = 80;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write with tiled LZ4
  auto path = GetTestPath("roundtrip_tiled_lz4.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path, TileSize(32, 32), CompressionType::kLZ4);
  sink.Render(source);

  // Read with source
  auto fimage_source = FImageSource::Create(path);

  // Read full image
  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, RoundTripTiledLZ4ChannelsFirst) {
  int width = 100;
  int height = 80;
  int channels = 3;
  auto original_data = CreatePatternImageDataCHW(width, height, channels);

  auto path = GetTestPath("roundtrip_tiled_lz4_chw.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsFirst),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path, TileSize(32, 32), CompressionType::kLZ4);
  sink.Render(source);

  auto fimage_source = FImageSource::Create(path);
  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, RoundTripContiguousZstd) {
  int width = 64;
  int height = 48;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write with Zstd compression
  auto path = GetTestPath("roundtrip_zstd.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path, TileSize(0, 0), CompressionType::kZstd);
  sink.Render(source);

  // Read with source
  auto fimage_source = FImageSource::Create(path);

  // Read full image
  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, RoundTripTiledZstd) {
  int width = 100;
  int height = 80;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write with tiled Zstd
  auto path = GetTestPath("roundtrip_tiled_zstd.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink =
      FImageSink::Create(path, TileSize(32, 32), CompressionType::kZstd);
  sink.Render(source);

  // Read with source
  auto fimage_source = FImageSource::Create(path);

  // Read full image
  auto tile = fimage_source.GetTile(0, 0, width, height);
  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(FImageSourceTest, PartialTileReading) {
  int width = 100;
  int height = 80;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write tiled
  auto path = GetTestPath("partial_tiles.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink =
      FImageSink::Create(path, TileSize(32, 32), CompressionType::kNone);
  sink.Render(source);

  // Read with source
  auto fimage_source = FImageSource::Create(path);

  // Read various regions
  auto tile1 = fimage_source.GetTile(10, 10, 20, 20);
  EXPECT_EQ(tile1.width, 20);
  EXPECT_EQ(tile1.height, 20);

  // Verify data matches original
  for (int y = 0; y < 20; ++y) {
    for (int x = 0; x < 20; ++x) {
      for (int c = 0; c < channels; ++c) {
        int tile_idx = (y * 20 + x) * channels + c;
        int orig_idx = ((10 + y) * width + (10 + x)) * channels + c;
        EXPECT_EQ(tile1.GetData()[tile_idx], original_data[orig_idx]);
      }
    }
  }
}

TEST_F(FImageSourceTest, BoundaryTiles) {
  int width = 100;
  int height = 80;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write tiled
  auto path = GetTestPath("boundary_tiles.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink =
      FImageSink::Create(path, TileSize(32, 32), CompressionType::kNone);
  sink.Render(source);

  // Read with source
  auto fimage_source = FImageSource::Create(path);

  // Read bottom-right corner (partial tile)
  auto tile = fimage_source.GetTile(90, 70, 10, 10);
  EXPECT_EQ(tile.width, 10);
  EXPECT_EQ(tile.height, 10);

  // Verify data
  for (int y = 0; y < 10; ++y) {
    for (int x = 0; x < 10; ++x) {
      for (int c = 0; c < channels; ++c) {
        int tile_idx = (y * 10 + x) * channels + c;
        int orig_idx = ((70 + y) * width + (90 + x)) * channels + c;
        EXPECT_EQ(tile.GetData()[tile_idx], original_data[orig_idx]);
      }
    }
  }
}

TEST_F(FImageSourceTest, MultiTileRead) {
  int width = 100;
  int height = 80;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write tiled
  auto path = GetTestPath("multi_tile.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path, TileSize(32, 32), CompressionType::kLZ4);
  sink.Render(source);

  // Read with source
  auto fimage_source = FImageSource::Create(path);

  // Read a region spanning multiple tiles (20, 20, 50, 40)
  auto tile = fimage_source.GetTile(20, 20, 50, 40);
  EXPECT_EQ(tile.width, 50);
  EXPECT_EQ(tile.height, 40);

  // Verify data matches original
  for (int y = 0; y < 40; ++y) {
    for (int x = 0; x < 50; ++x) {
      for (int c = 0; c < channels; ++c) {
        int tile_idx = (y * 50 + x) * channels + c;
        int orig_idx = ((20 + y) * width + (20 + x)) * channels + c;
        EXPECT_EQ(tile.GetData()[tile_idx], original_data[orig_idx]);
      }
    }
  }
}

TEST_F(FImageSourceTest, DifferentChannelCounts) {
  for (int channels : {1, 3, 4}) {
    int width = 64;
    int height = 48;
    auto original_data = CreatePatternImageData(width, height, channels);

    auto path = GetTestPath("channels_" + std::to_string(channels) + ".fimage");
    auto source = MemorySource::Create(
        std::vector<uint8_t>(original_data),
        ImageInfo(width, height, channels, PixelType::kUInt8,
                  DataLayout::kChannelsLast),
        TileSize(16, 16));
    auto sink =
        FImageSink::Create(path, TileSize(32, 32), CompressionType::kZstd);
    sink.Render(source);

    auto fimage_source = FImageSource::Create(path);
    EXPECT_EQ(fimage_source.GetChannels(), channels);

    auto tile = fimage_source.GetTile(0, 0, width, height);
    EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
  }
}

TEST_F(FImageSourceTest, MPPPreservation) {
  int width = 64;
  int height = 48;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);
  double mpp_x = 0.5;
  double mpp_y = 0.5;

  // Write with MPP
  auto path = GetTestPath("with_mpp.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path, TileSize(32, 32), CompressionType::kNone,
                                 mpp_x, mpp_y);
  sink.Render(source);

  // Read and verify MPP
  auto fimage_source = FImageSource::Create(path);
  EXPECT_DOUBLE_EQ(fimage_source.GetMppX(), mpp_x);
  EXPECT_DOUBLE_EQ(fimage_source.GetMppY(), mpp_y);
}

TEST_F(FImageSourceTest, ErrorInvalidFile) {
  auto path = GetTestPath("nonexistent.fimage");
  EXPECT_THROW(FImageSource::Create(path), std::runtime_error);
}

TEST_F(FImageSourceTest, ErrorCorruptedHeader) {
  auto path = GetTestPath("corrupted.fimage");

  // Write invalid header
  std::ofstream out(path, std::ios::binary);
  std::vector<uint8_t> bad_data(256, 0xFF);
  out.write(reinterpret_cast<const char*>(bad_data.data()), 256);
  out.close();

  EXPECT_THROW(FImageSource::Create(path), std::runtime_error);
}

TEST_F(FImageSourceTest, ErrorOutOfBoundsTile) {
  int width = 64;
  int height = 48;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  auto path = GetTestPath("oob.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path);
  sink.Render(source);

  auto fimage_source = FImageSource::Create(path);

  // Request out of bounds
  EXPECT_THROW(fimage_source.GetTile(0, 0, width + 10, height),
               std::runtime_error);
  EXPECT_THROW(fimage_source.GetTile(-10, 0, 10, 10), std::runtime_error);
}

TEST_F(FImageSourceTest, MoveSemantics) {
  int width = 64;
  int height = 48;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  auto path = GetTestPath("move_test.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));
  auto sink = FImageSink::Create(path);
  sink.Render(source);

  // Test move constructor
  auto source1 = FImageSource::Create(path);
  auto source2 = std::move(source1);

  auto tile = source2.GetTile(0, 0, 32, 32);
  EXPECT_EQ(tile.width, 32);
  EXPECT_EQ(tile.height, 32);

  // Test move assignment
  auto source3 = FImageSource::Create(path);
  source3 = std::move(source2);

  tile = source3.GetTile(0, 0, 32, 32);
  EXPECT_EQ(tile.width, 32);
  EXPECT_EQ(tile.height, 32);
}

}  // namespace
}  // namespace fim

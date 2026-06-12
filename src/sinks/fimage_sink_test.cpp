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
 * @file fimage_sink_test.cpp
 * @brief Tests for FImage native format sink.
 * @author Jonas Teuwen
 * @date 2025
 */
#include "fim/sinks/fimage_sink.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>

#include "fim/sources/fimage_source.h"
#include "fim/sources/memory_source.h"

namespace fim {
namespace {

// Helper to create test image data
std::vector<uint8_t> CreateTestImageData(int width, int height, int channels) {
  std::vector<uint8_t> data(width * height * channels);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>(i % 256);
  }
  return data;
}

// Helper to read and validate header
FImageHeader ReadHeader(const fs::path& filename) {
  std::ifstream in(filename, std::ios::binary);
  EXPECT_TRUE(in.is_open());

  FImageHeader header;
  EXPECT_TRUE(header.ReadFrom(in));
  EXPECT_TRUE(header.IsValid());

  return header;
}

// Test fixture
class FImageSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = fs::temp_directory_path() / "fimage_sink_test";
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

TEST_F(FImageSinkTest, HeaderSerialization) {
  FImageHeader header;
  header.width = 1024;
  header.height = 768;
  header.channels = 3;
  header.pixel_type = static_cast<std::uint8_t>(PixelType::kUInt8);
  header.data_layout = static_cast<std::uint8_t>(DataLayout::kChannelsLast);
  header.compression = static_cast<std::uint8_t>(CompressionType::kNone);
  header.tile_width = 256;
  header.tile_height = 256;
  header.num_tiles_x = 4;
  header.num_tiles_y = 3;
  header.mpp_x = 0.5;
  header.mpp_y = 0.5;
  header.data_offset = 256;
  header.seek_table_offset = 0;

  // Write header
  auto path = GetTestPath("header_test.fimage");
  std::ofstream out(path, std::ios::binary);
  header.WriteTo(out);
  out.close();

  // Read header back
  FImageHeader read_header = ReadHeader(path);

  // Verify all fields
  EXPECT_TRUE(read_header.IsValid());
  EXPECT_EQ(read_header.width, 1024u);
  EXPECT_EQ(read_header.height, 768u);
  EXPECT_EQ(read_header.channels, 3u);
  EXPECT_EQ(read_header.pixel_type,
            static_cast<std::uint8_t>(PixelType::kUInt8));
  EXPECT_EQ(read_header.data_layout,
            static_cast<std::uint8_t>(DataLayout::kChannelsLast));
  EXPECT_EQ(read_header.compression,
            static_cast<std::uint8_t>(CompressionType::kNone));
  EXPECT_EQ(read_header.tile_width, 256u);
  EXPECT_EQ(read_header.tile_height, 256u);
  EXPECT_EQ(read_header.num_tiles_x, 4u);
  EXPECT_EQ(read_header.num_tiles_y, 3u);
  EXPECT_DOUBLE_EQ(read_header.mpp_x, 0.5);
  EXPECT_DOUBLE_EQ(read_header.mpp_y, 0.5);
  EXPECT_EQ(read_header.data_offset, 256u);
  EXPECT_EQ(read_header.seek_table_offset, 0u);
}

TEST_F(FImageSinkTest, ContiguousUncompressed) {
  // Create test data
  int width = 64;
  int height = 48;
  int channels = 3;
  auto data = CreateTestImageData(width, height, channels);

  // Create memory source
  auto source =
      MemorySource::Create(std::move(data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));

  // Write FImage
  auto path = GetTestPath("contiguous_uncompressed.fimage");
  auto sink = FImageSink::Create(path);
  sink.Render(source);

  // Verify file exists and has correct header
  EXPECT_TRUE(fs::exists(path));
  auto header = ReadHeader(path);

  EXPECT_EQ(header.width, static_cast<std::uint32_t>(width));
  EXPECT_EQ(header.height, static_cast<std::uint32_t>(height));
  EXPECT_EQ(header.channels, static_cast<std::uint32_t>(channels));
  EXPECT_EQ(header.tile_width, 0u);  // Contiguous mode
  EXPECT_EQ(header.tile_height, 0u);
  EXPECT_EQ(header.compression,
            static_cast<std::uint8_t>(CompressionType::kNone));
  EXPECT_EQ(header.seek_table_offset, 0u);  // No seek table for uncompressed

  // Verify file size
  size_t expected_size = 256 + width * height * channels;
  EXPECT_EQ(fs::file_size(path), expected_size);
}

TEST_F(FImageSinkTest, TiledUncompressed) {
  // Create test data
  int width = 100;
  int height = 80;
  int channels = 3;
  auto data = CreateTestImageData(width, height, channels);

  // Create memory source
  auto source =
      MemorySource::Create(std::move(data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));

  // Write tiled FImage
  auto path = GetTestPath("tiled_uncompressed.fimage");
  auto sink =
      FImageSink::Create(path, TileSize(32, 32), CompressionType::kNone);
  sink.Render(source);

  // Verify header
  EXPECT_TRUE(fs::exists(path));
  auto header = ReadHeader(path);

  EXPECT_EQ(header.width, static_cast<std::uint32_t>(width));
  EXPECT_EQ(header.height, static_cast<std::uint32_t>(height));
  EXPECT_EQ(header.channels, static_cast<std::uint32_t>(channels));
  EXPECT_EQ(header.tile_width, 32u);
  EXPECT_EQ(header.tile_height, 32u);

  // Calculate expected number of tiles
  size_t expected_tiles_x = (width + 31) / 32;   // 4 tiles
  size_t expected_tiles_y = (height + 31) / 32;  // 3 tiles
  EXPECT_EQ(header.num_tiles_x, static_cast<std::uint32_t>(expected_tiles_x));
  EXPECT_EQ(header.num_tiles_y, static_cast<std::uint32_t>(expected_tiles_y));

  EXPECT_EQ(header.compression,
            static_cast<std::uint8_t>(CompressionType::kNone));
  EXPECT_EQ(header.seek_table_offset, 0u);

  // Verify file size: header + (num_tiles * tile_size)
  // Each tile is 32x32x3 = 3072 bytes (padded to full tile size)
  size_t expected_size =
      256 + (expected_tiles_x * expected_tiles_y * 32 * 32 * channels);
  EXPECT_EQ(fs::file_size(path), expected_size);
}

TEST_F(FImageSinkTest, ContiguousLZ4Compressed) {
  // Create test data
  int width = 64;
  int height = 48;
  int channels = 3;
  auto data = CreateTestImageData(width, height, channels);

  // Create memory source
  auto source =
      MemorySource::Create(std::move(data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));

  // Write compressed FImage
  auto path = GetTestPath("contiguous_lz4.fimage");
  auto sink = FImageSink::Create(path, TileSize(0, 0), CompressionType::kLZ4);
  sink.Render(source);

  // Verify header
  EXPECT_TRUE(fs::exists(path));
  auto header = ReadHeader(path);

  EXPECT_EQ(header.compression,
            static_cast<std::uint8_t>(CompressionType::kLZ4));
  EXPECT_GT(header.seek_table_offset, 256u);  // Seek table should exist

  // Verify seek table exists
  std::ifstream in(path, std::ios::binary);
  in.seekg(header.seek_table_offset);
  SeekTableEntry entry;
  EXPECT_TRUE(entry.ReadFrom(in));
  EXPECT_EQ(entry.offset, 256u);  // Data starts after header
  EXPECT_GT(entry.compressed_size, 0u);
}

TEST_F(FImageSinkTest, TiledLZ4Compressed) {
  // Create test data
  int width = 100;
  int height = 80;
  int channels = 3;
  auto data = CreateTestImageData(width, height, channels);

  // Create memory source
  auto source =
      MemorySource::Create(std::move(data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));

  // Write tiled compressed FImage
  auto path = GetTestPath("tiled_lz4.fimage");
  auto sink = FImageSink::Create(path, TileSize(32, 32), CompressionType::kLZ4);
  sink.Render(source);

  // Verify header
  EXPECT_TRUE(fs::exists(path));
  auto header = ReadHeader(path);

  EXPECT_EQ(header.tile_width, 32u);
  EXPECT_EQ(header.tile_height, 32u);
  EXPECT_EQ(header.compression,
            static_cast<std::uint8_t>(CompressionType::kLZ4));
  EXPECT_GT(header.seek_table_offset, 256u);

  // Verify seek table has correct number of entries
  size_t num_tiles = header.num_tiles_x * header.num_tiles_y;
  std::ifstream in(path, std::ios::binary);
  in.seekg(header.seek_table_offset);

  for (size_t i = 0; i < num_tiles; ++i) {
    SeekTableEntry entry;
    EXPECT_TRUE(entry.ReadFrom(in));
    EXPECT_GT(entry.compressed_size, 0u);
  }
}

TEST_F(FImageSinkTest, ContiguousZstdCompressed) {
  // Create test data
  int width = 64;
  int height = 48;
  int channels = 3;
  auto data = CreateTestImageData(width, height, channels);

  // Create memory source
  auto source =
      MemorySource::Create(std::move(data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));

  // Write Zstd compressed FImage
  auto path = GetTestPath("contiguous_zstd.fimage");
  auto sink = FImageSink::Create(path, TileSize(0, 0), CompressionType::kZstd);
  sink.Render(source);

  // Verify header
  EXPECT_TRUE(fs::exists(path));
  auto header = ReadHeader(path);

  EXPECT_EQ(header.compression,
            static_cast<std::uint8_t>(CompressionType::kZstd));
  EXPECT_GT(header.seek_table_offset, 256u);
}

TEST_F(FImageSinkTest, TiledZstdCompressed) {
  // Create test data
  int width = 100;
  int height = 80;
  int channels = 3;
  auto data = CreateTestImageData(width, height, channels);

  // Create memory source
  auto source =
      MemorySource::Create(std::move(data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));

  // Write tiled Zstd compressed FImage
  auto path = GetTestPath("tiled_zstd.fimage");
  auto sink =
      FImageSink::Create(path, TileSize(32, 32), CompressionType::kZstd);
  sink.Render(source);

  // Verify header
  EXPECT_TRUE(fs::exists(path));
  auto header = ReadHeader(path);

  EXPECT_EQ(header.tile_width, 32u);
  EXPECT_EQ(header.tile_height, 32u);
  EXPECT_EQ(header.compression,
            static_cast<std::uint8_t>(CompressionType::kZstd));
  EXPECT_GT(header.seek_table_offset, 256u);
}

TEST_F(FImageSinkTest, WithMPPMetadata) {
  // Create test data
  int width = 64;
  int height = 48;
  int channels = 3;
  auto data = CreateTestImageData(width, height, channels);

  // Create memory source
  auto source =
      MemorySource::Create(std::move(data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(16, 16));

  // Write with MPP metadata
  auto path = GetTestPath("with_mpp.fimage");
  double mpp_x = 0.25;
  double mpp_y = 0.25;
  auto sink = FImageSink::Create(path, TileSize(32, 32), CompressionType::kNone,
                                 mpp_x, mpp_y);
  sink.Render(source);

  // Verify MPP in header
  auto header = ReadHeader(path);
  EXPECT_DOUBLE_EQ(header.mpp_x, mpp_x);
  EXPECT_DOUBLE_EQ(header.mpp_y, mpp_y);
}

TEST_F(FImageSinkTest, DifferentChannelCounts) {
  for (int channels : {1, 3, 4}) {
    int width = 64;
    int height = 48;
    auto data = CreateTestImageData(width, height, channels);

    auto source = MemorySource::Create(
        std::move(data),
        ImageInfo(width, height, channels, PixelType::kUInt8,
                  DataLayout::kChannelsLast),
        TileSize(16, 16));

    auto path = GetTestPath("channels_" + std::to_string(channels) + ".fimage");
    auto sink = FImageSink::Create(path, TileSize(32, 32));
    sink.Render(source);

    auto header = ReadHeader(path);
    EXPECT_EQ(header.channels, static_cast<std::uint32_t>(channels));
  }
}

TEST_F(FImageSinkTest, LargeImage) {
  // Test with a larger image to ensure proper handling
  int width = 512;
  int height = 512;
  int channels = 3;
  auto data = CreateTestImageData(width, height, channels);

  auto source =
      MemorySource::Create(std::move(data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(64, 64));

  auto path = GetTestPath("large.fimage");
  auto sink =
      FImageSink::Create(path, TileSize(128, 128), CompressionType::kZstd);
  sink.Render(source);

  auto header = ReadHeader(path);
  EXPECT_EQ(header.width, static_cast<std::uint32_t>(width));
  EXPECT_EQ(header.height, static_cast<std::uint32_t>(height));
  EXPECT_EQ(header.num_tiles_x, 4u);  // 512 / 128 = 4
  EXPECT_EQ(header.num_tiles_y, 4u);
}

// Test uint16 with LZ4 compression round-trip
TEST_F(FImageSinkTest, UInt16CompressionRoundTrip) {
  int width = 64;
  int height = 48;
  int channels = 3;

  // Create uint16 test data
  std::vector<uint16_t> data_uint16(width * height * channels);
  for (size_t i = 0; i < data_uint16.size(); ++i) {
    data_uint16[i] = static_cast<uint16_t>(i * 100);
  }

  ImageInfo dims(width, height, channels, PixelType::kUInt16,
                 DataLayout::kChannelsLast);
  auto source = MemorySource::CreateTyped(std::move(data_uint16), width, height,
                                          channels);

  // Write with LZ4 compression
  auto path = GetTestPath("uint16_lz4_compressed.fimage");
  auto sink = FImageSink::Create(path, TileSize(0, 0), CompressionType::kLZ4);
  sink.Render(source);

  // Verify header
  auto header = ReadHeader(path);
  EXPECT_EQ(header.pixel_type, static_cast<std::uint8_t>(PixelType::kUInt16));
  EXPECT_EQ(header.compression,
            static_cast<std::uint8_t>(CompressionType::kLZ4));

  // Read back and verify
  auto loaded_source = FImageSource::Create(path);
  auto loaded_dims = loaded_source.GetDimensions();
  EXPECT_EQ(loaded_dims.pixel_type, PixelType::kUInt16);

  auto tile = loaded_source.GetTile(0, 0, width, height);
  EXPECT_EQ(tile.GetPixelType(), PixelType::kUInt16);
  const auto& loaded_data = tile.GetDataAs<uint16_t>();
  EXPECT_EQ(loaded_data.size(), width * height * channels);

  // Verify first few values
  EXPECT_EQ(loaded_data[0], 0);
  EXPECT_EQ(loaded_data[1], 100);
  EXPECT_EQ(loaded_data[2], 200);
}

// Test float32 with ZSTD compression round-trip
TEST_F(FImageSinkTest, Float32CompressionRoundTrip) {
  int width = 32;
  int height = 24;
  int channels = 1;

  // Create float32 test data
  std::vector<float> data_float(width * height * channels);
  for (size_t i = 0; i < data_float.size(); ++i) {
    data_float[i] = static_cast<float>(i) * 0.5f;
  }

  ImageInfo dims(width, height, channels, PixelType::kFloat32,
                 DataLayout::kChannelsLast);
  auto source =
      MemorySource::CreateTyped(std::move(data_float), width, height, channels);

  // Write with ZSTD compression
  auto path = GetTestPath("float32_zstd_compressed.fimage");
  auto sink = FImageSink::Create(path, TileSize(0, 0), CompressionType::kZstd);
  sink.Render(source);

  // Verify header
  auto header = ReadHeader(path);
  EXPECT_EQ(header.pixel_type, static_cast<std::uint8_t>(PixelType::kFloat32));
  EXPECT_EQ(header.compression,
            static_cast<std::uint8_t>(CompressionType::kZstd));

  // Read back and verify
  auto loaded_source = FImageSource::Create(path);
  auto loaded_dims = loaded_source.GetDimensions();
  EXPECT_EQ(loaded_dims.pixel_type, PixelType::kFloat32);

  auto tile = loaded_source.GetTile(0, 0, width, height);
  EXPECT_EQ(tile.GetPixelType(), PixelType::kFloat32);
  const auto& loaded_data = tile.GetDataAs<float>();
  EXPECT_EQ(loaded_data.size(), width * height * channels);

  // Verify first few values
  EXPECT_FLOAT_EQ(loaded_data[0], 0.0f);
  EXPECT_FLOAT_EQ(loaded_data[1], 0.5f);
  EXPECT_FLOAT_EQ(loaded_data[2], 1.0f);
}

}  // namespace
}  // namespace fim

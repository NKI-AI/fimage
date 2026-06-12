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

#include <fim/types.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace fim {

class TileTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

// Test Tile default construction
TEST_F(TileTest, DefaultConstruction) {
  Tile tile;
  EXPECT_EQ(tile.x, 0);
  EXPECT_EQ(tile.y, 0);
  EXPECT_EQ(tile.width, 0);
  EXPECT_EQ(tile.height, 0);
  EXPECT_EQ(tile.channels, 0);
  EXPECT_EQ(tile.expected_width, 0);
  EXPECT_EQ(tile.expected_height, 0);
  EXPECT_FALSE(tile.needs_padding);
  EXPECT_TRUE(tile.GetData().empty());
}

// Test Tile construction with basic parameters
TEST_F(TileTest, BasicConstruction) {
  Tile tile(10, 20, 100, 200, 3);
  EXPECT_EQ(tile.x, 10);
  EXPECT_EQ(tile.y, 20);
  EXPECT_EQ(tile.width, 100);
  EXPECT_EQ(tile.height, 200);
  EXPECT_EQ(tile.channels, 3);
  EXPECT_EQ(tile.expected_width, 0);
  EXPECT_EQ(tile.expected_height, 0);
  EXPECT_FALSE(tile.needs_padding);
  EXPECT_EQ(tile.GetData().size(), 100 * 200 * 3);
}

// Test Tile construction with padding parameters
TEST_F(TileTest, PaddingConstruction) {
  Tile tile(10, 20, 80, 90, 4, 100, 100);
  EXPECT_EQ(tile.x, 10);
  EXPECT_EQ(tile.y, 20);
  EXPECT_EQ(tile.width, 80);
  EXPECT_EQ(tile.height, 90);
  EXPECT_EQ(tile.channels, 4);
  EXPECT_EQ(tile.expected_width, 100);
  EXPECT_EQ(tile.expected_height, 100);
  EXPECT_TRUE(tile.needs_padding);
  EXPECT_EQ(tile.GetData().size(), 80 * 90 * 4);
}

// Test Tile construction with no padding needed
TEST_F(TileTest, NoPaddingConstruction) {
  Tile tile(10, 20, 100, 100, 3, 100, 100);
  EXPECT_EQ(tile.x, 10);
  EXPECT_EQ(tile.y, 20);
  EXPECT_EQ(tile.width, 100);
  EXPECT_EQ(tile.height, 100);
  EXPECT_EQ(tile.channels, 3);
  EXPECT_EQ(tile.expected_width, 100);
  EXPECT_EQ(tile.expected_height, 100);
  EXPECT_FALSE(tile.needs_padding);
  EXPECT_EQ(tile.GetData().size(), 100 * 100 * 3);
}

// Test GetDataSize method
TEST_F(TileTest, GetDataSize) {
  Tile tile(0, 0, 10, 20, 3);
  EXPECT_EQ(tile.GetDataSize(), 10 * 20 * 3);
}

// Test GetExpectedDataSize method without padding
TEST_F(TileTest, GetExpectedDataSizeWithoutPadding) {
  Tile tile(0, 0, 10, 20, 3);
  EXPECT_EQ(tile.GetExpectedDataSize(), 10 * 20 * 3);
}

// Test GetExpectedDataSize method with padding
TEST_F(TileTest, GetExpectedDataSizeWithPadding) {
  Tile tile(0, 0, 10, 20, 3, 25, 30);
  EXPECT_EQ(tile.GetExpectedDataSize(), 25 * 30 * 3);
}

// Test CreatePaddedData method without padding
TEST_F(TileTest, CreatePaddedDataWithoutPadding) {
  Tile tile(0, 0, 2, 2, 1);
  // Fill with test data
  tile.GetDataMut() = {1, 2, 3, 4};

  auto padded_variant = tile.CreatePaddedData();
  // For uint8 tiles, get the uint8_t vector
  auto& padded = std::get<std::vector<uint8_t>>(padded_variant);
  EXPECT_EQ(padded.size(), 4);
  EXPECT_EQ(padded[0], 1);
  EXPECT_EQ(padded[1], 2);
  EXPECT_EQ(padded[2], 3);
  EXPECT_EQ(padded[3], 4);
}

// Test CreatePaddedData method with padding
TEST_F(TileTest, CreatePaddedDataWithPadding) {
  Tile tile(0, 0, 2, 2, 1, 3, 3);
  // Fill with test data
  tile.GetDataMut() = {1, 2, 3, 4};

  auto padded_variant = tile.CreatePaddedData();
  // For uint8 tiles, get the uint8_t vector
  auto& padded = std::get<std::vector<uint8_t>>(padded_variant);
  EXPECT_EQ(padded.size(), 9);

  // Check that the original data is in the top-left corner
  EXPECT_EQ(padded[0], 1);  // (0,0)
  EXPECT_EQ(padded[1], 2);  // (0,1)
  EXPECT_EQ(padded[2], 0);  // (0,2) - padding
  EXPECT_EQ(padded[3], 3);  // (1,0)
  EXPECT_EQ(padded[4], 4);  // (1,1)
  EXPECT_EQ(padded[5], 0);  // (1,2) - padding
  EXPECT_EQ(padded[6], 0);  // (2,0) - padding
  EXPECT_EQ(padded[7], 0);  // (2,1) - padding
  EXPECT_EQ(padded[8], 0);  // (2,2) - padding
}

// Test CreatePaddedData method with multi-channel data
TEST_F(TileTest, CreatePaddedDataMultiChannel) {
  Tile tile(0, 0, 2, 2, 2, 3, 3);
  // Fill with test data (2x2 image with 2 channels)
  tile.GetDataMut() = {1, 2, 3, 4, 5, 6, 7, 8};

  auto padded_variant = tile.CreatePaddedData();
  // For uint8 tiles, get the uint8_t vector
  auto& padded = std::get<std::vector<uint8_t>>(padded_variant);
  EXPECT_EQ(padded.size(), 18);  // 3x3x2

  // Check that the original data is in the top-left corner
  EXPECT_EQ(padded[0], 1);   // (0,0) channel 0
  EXPECT_EQ(padded[1], 2);   // (0,0) channel 1
  EXPECT_EQ(padded[2], 3);   // (0,1) channel 0
  EXPECT_EQ(padded[3], 4);   // (0,1) channel 1
  EXPECT_EQ(padded[4], 0);   // (0,2) channel 0 - padding
  EXPECT_EQ(padded[5], 0);   // (0,2) channel 1 - padding
  EXPECT_EQ(padded[6], 5);   // (1,0) channel 0
  EXPECT_EQ(padded[7], 6);   // (1,0) channel 1
  EXPECT_EQ(padded[8], 7);   // (1,1) channel 0
  EXPECT_EQ(padded[9], 8);   // (1,1) channel 1
  EXPECT_EQ(padded[10], 0);  // (1,2) channel 0 - padding
  EXPECT_EQ(padded[11], 0);  // (1,2) channel 1 - padding
  // Rest should be padding (zeros)
  for (int i = 12; i < 18; ++i) {
    EXPECT_EQ(padded[i], 0);
  }
}

class ImageInfoTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

// Test ImageInfo default construction
TEST_F(ImageInfoTest, DefaultConstruction) {
  ImageInfo dims;
  EXPECT_EQ(dims.GetWidth(), 0);
  EXPECT_EQ(dims.GetHeight(), 0);
  EXPECT_EQ(dims.channels, 0);
}

// Test ImageInfo construction with parameters
TEST_F(ImageInfoTest, ParameterizedConstruction) {
  ImageInfo dims(800, 600, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  EXPECT_EQ(dims.GetWidth(), 800);
  EXPECT_EQ(dims.GetHeight(), 600);
  EXPECT_EQ(dims.channels, 3);
}

// Test ImageInfo with various channel counts
TEST_F(ImageInfoTest, VariousChannelCounts) {
  ImageInfo grayscale(100, 100, 1, PixelType::kUInt8,
                      DataLayout::kChannelsLast);
  ImageInfo rgb(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  ImageInfo rgba(100, 100, 4, PixelType::kUInt8, DataLayout::kChannelsLast);

  EXPECT_EQ(grayscale.channels, 1);
  EXPECT_EQ(rgb.channels, 3);
  EXPECT_EQ(rgba.channels, 4);
}

class TileSizeTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

// Test TileSize default construction
TEST_F(TileSizeTest, DefaultConstruction) {
  TileSize tile_size;
  EXPECT_EQ(tile_size.width, 0);
  EXPECT_EQ(tile_size.height, 0);
}

// Test TileSize construction with parameters
TEST_F(TileSizeTest, ParameterizedConstruction) {
  TileSize tile_size(256, 256);
  EXPECT_EQ(tile_size.width, 256);
  EXPECT_EQ(tile_size.height, 256);
}

// Test TileSize with different dimensions
TEST_F(TileSizeTest, DifferentDimensions) {
  TileSize tile_size(512, 256);
  EXPECT_EQ(tile_size.width, 512);
  EXPECT_EQ(tile_size.height, 256);
}

// Test TileSize with zero dimensions
TEST_F(TileSizeTest, ZeroDimensions) {
  TileSize tile_size(0, 0);
  EXPECT_EQ(tile_size.width, 0);
  EXPECT_EQ(tile_size.height, 0);
}

// Test for overflow protection: Large tile dimensions
TEST_F(TileTest, LargeTileNoOverflow) {
  // Test that large dimensions don't cause integer overflow
  // 50000 * 50000 * 3 = 7,500,000,000 which exceeds int32 max (2,147,483,647)
  // but should work correctly with size_t casting
  Tile tile(0, 0, 50000, 50000, 3);

  // Verify the tile was constructed
  EXPECT_EQ(tile.width, 50000);
  EXPECT_EQ(tile.height, 50000);
  EXPECT_EQ(tile.channels, 3);

  // Check GetDataSize returns correct value (as size_t, not overflowed int)
  size_t expected_size = static_cast<size_t>(50000) *
                         static_cast<size_t>(50000) * static_cast<size_t>(3);
  EXPECT_EQ(tile.GetDataSize(), expected_size);
  EXPECT_GT(tile.GetDataSize(),
            static_cast<size_t>(std::numeric_limits<int>::max()));
}

// Test for overflow protection: GetExpectedDataSize with large dimensions
TEST_F(TileTest, LargeExpectedDataSizeNoOverflow) {
  // Test GetExpectedDataSize with large dimensions
  Tile tile(0, 0, 49000, 49000, 3, 50000, 50000);

  size_t expected_size = static_cast<size_t>(50000) *
                         static_cast<size_t>(50000) * static_cast<size_t>(3);
  EXPECT_EQ(tile.GetExpectedDataSize(), expected_size);
  EXPECT_GT(tile.GetExpectedDataSize(),
            static_cast<size_t>(std::numeric_limits<int>::max()));
}

// Test lazy tile with zero dimensions doesn't re-invoke producer
TEST_F(TileTest, LazyTileZeroDimensionsNoReinvoke) {
  int producer_call_count = 0;

  // Create a lazy tile with zero dimensions (edge case)
  Tile tile(0, 0, 0, 0, 3, [&producer_call_count]() -> TileData {
    producer_call_count++;
    return std::vector<uint8_t>();
  });

  // Call GetVariantData multiple times
  tile.GetVariantData();
  tile.GetVariantData();
  tile.GetVariantData();

  // Producer should only be called once, not multiple times
  EXPECT_EQ(producer_call_count, 1);
}

// Test lazy tile producer is called exactly once
TEST_F(TileTest, LazyTileProducerCalledOnce) {
  int producer_call_count = 0;

  // Create a lazy tile with a producer
  Tile tile(0, 0, 10, 10, 3, [&producer_call_count]() -> TileData {
    producer_call_count++;
    std::vector<uint8_t> data(10 * 10 * 3, 42);
    return data;
  });

  // Call GetVariantData multiple times
  const auto& data1 = tile.GetVariantData();
  tile.GetVariantData();  // Second call should not invoke producer

  // Producer should only be called once
  EXPECT_EQ(producer_call_count, 1);

  // Verify data is correct
  const auto& vec = std::get<std::vector<uint8_t>>(data1);
  EXPECT_EQ(vec.size(), 300);
  EXPECT_EQ(vec[0], 42);
}

// Test lazy tile with uint16 type
TEST_F(TileTest, LazyTileUInt16) {
  int producer_call_count = 0;

  Tile tile(
      0, 0, 5, 5, 1,
      [&producer_call_count]() -> TileData {
        producer_call_count++;
        std::vector<uint16_t> data(5 * 5, 1000);
        return data;
      },
      DataLayout::kChannelsLast, PixelType::kUInt16);

  // Access data multiple times
  tile.GetDataAs<uint16_t>();
  tile.GetDataAs<uint16_t>();

  // Producer should only be called once
  EXPECT_EQ(producer_call_count, 1);
}

// Test lazy tile with float type
TEST_F(TileTest, LazyTileFloat32) {
  int producer_call_count = 0;

  Tile tile(
      0, 0, 8, 8, 3,
      [&producer_call_count]() -> TileData {
        producer_call_count++;
        std::vector<float> data(8 * 8 * 3, 3.14f);
        return data;
      },
      DataLayout::kChannelsLast, PixelType::kFloat32);

  // Access data multiple times
  const auto& data = tile.GetDataAs<float>();
  tile.GetDataAs<float>();

  // Producer should only be called once
  EXPECT_EQ(producer_call_count, 1);
  EXPECT_EQ(data.size(), 192);
  EXPECT_FLOAT_EQ(data[0], 3.14f);
}

// Test thread-safe lazy materialization
TEST_F(TileTest, ThreadSafeLazyMaterialization) {
  std::atomic<int> producer_call_count{0};

  // Create a lazy tile
  Tile tile(0, 0, 100, 100, 3, [&producer_call_count]() -> TileData {
    producer_call_count.fetch_add(1, std::memory_order_relaxed);
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::vector<uint8_t> data(100 * 100 * 3, 123);
    return data;
  });

  // Access the same tile from multiple threads concurrently
  constexpr int kNumThreads = 10;
  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&tile]() {
      // Multiple threads try to materialize simultaneously
      const auto& data = tile.GetVariantData();
      // Verify data is correct
      const auto& vec = std::get<std::vector<uint8_t>>(data);
      EXPECT_EQ(vec.size(), 30000);
      EXPECT_EQ(vec[0], 123);
    });
  }

  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }

  // Producer should have been called exactly once, despite concurrent access
  EXPECT_EQ(producer_call_count.load(), 1);
}

}  // namespace fim

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
 * @file memory_sink_test.cpp
 * @brief Tests for MemorySink.
 * @author Jonas Teuwen
 * @date 2025
 */
#include "fim/sinks/memory_sink.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "fim/sources/memory_source.h"

namespace fim {
namespace {

// Helper to create test image data
std::vector<uint8_t> CreateTestImageData(int width, int height, int channels) {
  std::vector<uint8_t> data(static_cast<size_t>(width) *
                            static_cast<size_t>(height) *
                            static_cast<size_t>(channels));
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>(i % 256);
  }
  return data;
}

// Test fixture
class MemorySinkTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

// Basic test: Small image rendering
TEST_F(MemorySinkTest, SmallImageRendering) {
  // Create test data
  int width = 100;
  int height = 80;
  int channels = 3;
  auto test_data = CreateTestImageData(width, height, channels);

  // Create memory source
  auto source =
      MemorySource::CreateTyped(test_data, width, height, channels,
                                DataLayout::kChannelsLast, TileSize(32, 32));

  // Render to memory sink
  auto sink = MemorySink::Create();
  sink.Render(source);

  // Verify dimensions
  const auto& dims = sink.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), width);
  EXPECT_EQ(dims.GetHeight(), height);
  EXPECT_EQ(dims.channels, channels);

  // Verify data
  const auto& rendered_data = sink.GetDataAs<uint8_t>();
  ASSERT_EQ(rendered_data.size(), test_data.size());
  for (size_t i = 0; i < test_data.size(); ++i) {
    EXPECT_EQ(rendered_data[i], test_data[i]);
  }
}

// Test parallel assembly with larger image
TEST_F(MemorySinkTest, ParallelAssemblyLargeImage) {
  // Create a larger image that will trigger parallel processing
  int width = 2048;
  int height = 2048;
  int channels = 3;
  auto test_data = CreateTestImageData(width, height, channels);

  // Create memory source with small tiles to generate many tiles
  auto source =
      MemorySource::CreateTyped(test_data, width, height, channels,
                                DataLayout::kChannelsLast, TileSize(256, 256));

  // Render to memory sink (should use parallel processing)
  auto sink = MemorySink::Create();
  sink.Render(source);

  // Verify dimensions
  const auto& dims = sink.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), width);
  EXPECT_EQ(dims.GetHeight(), height);
  EXPECT_EQ(dims.channels, channels);

  // Verify all data matches
  const auto& rendered_data = sink.GetDataAs<uint8_t>();
  ASSERT_EQ(rendered_data.size(), test_data.size());

  // Check a sample of pixels to ensure correct assembly
  for (int y = 0; y < height; y += 100) {
    for (int x = 0; x < width; x += 100) {
      for (int c = 0; c < channels; ++c) {
        size_t idx = (y * width + x) * channels + c;
        EXPECT_EQ(rendered_data[idx], test_data[idx])
            << "Mismatch at (" << x << ", " << y << ", " << c << ")";
      }
    }
  }
}

// Test with uint16 pixel type
TEST_F(MemorySinkTest, UInt16PixelType) {
  int width = 512;
  int height = 512;
  int channels = 1;

  std::vector<uint16_t> test_data(static_cast<size_t>(width) *
                                  static_cast<size_t>(height) * channels);
  for (size_t i = 0; i < test_data.size(); ++i) {
    test_data[i] = static_cast<uint16_t>(i % 65536);
  }

  auto source =
      MemorySource::CreateTyped(test_data, width, height, channels,
                                DataLayout::kChannelsLast, TileSize(128, 128));

  auto sink = MemorySink::Create();
  sink.Render(source);

  const auto& rendered_data = sink.GetDataAs<uint16_t>();
  ASSERT_EQ(rendered_data.size(), test_data.size());

  for (size_t i = 0; i < test_data.size(); ++i) {
    EXPECT_EQ(rendered_data[i], test_data[i]);
  }
}

// Test with float pixel type
TEST_F(MemorySinkTest, Float32PixelType) {
  int width = 256;
  int height = 256;
  int channels = 3;

  std::vector<float> test_data(static_cast<size_t>(width) *
                               static_cast<size_t>(height) * channels);
  for (size_t i = 0; i < test_data.size(); ++i) {
    test_data[i] = static_cast<float>(i) / 1000.0f;
  }

  auto source =
      MemorySource::CreateTyped(test_data, width, height, channels,
                                DataLayout::kChannelsLast, TileSize(64, 64));

  auto sink = MemorySink::Create();
  sink.Render(source);

  const auto& rendered_data = sink.GetDataAs<float>();
  ASSERT_EQ(rendered_data.size(), test_data.size());

  for (size_t i = 0; i < test_data.size(); ++i) {
    EXPECT_FLOAT_EQ(rendered_data[i], test_data[i]);
  }
}

// Test large dimensions to verify overflow protection
TEST_F(MemorySinkTest, LargeDimensionsNoOverflow) {
  // Create a large image (but not so large it runs out of memory in tests)
  // 10000 * 10000 * 3 = 300,000,000 bytes ~= 286 MB
  int width = 10000;
  int height = 10000;
  int channels = 3;

  auto test_data = CreateTestImageData(width, height, channels);

  auto source =
      MemorySource::CreateTyped(test_data, width, height, channels,
                                DataLayout::kChannelsLast, TileSize(512, 512));

  auto sink = MemorySink::Create();
  sink.Render(source);

  const auto& dims = sink.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), width);
  EXPECT_EQ(dims.GetHeight(), height);
  EXPECT_EQ(dims.channels, channels);

  const auto& rendered_data = sink.GetDataAs<uint8_t>();

  // Verify size is correct (would be wrong if overflow occurred)
  size_t expected_size = static_cast<size_t>(width) *
                         static_cast<size_t>(height) *
                         static_cast<size_t>(channels);
  EXPECT_EQ(rendered_data.size(), expected_size);
  // The test verifies that the calculation didn't overflow by checking the size
  // is correct
  EXPECT_EQ(rendered_data.size(), 300000000);
}

// Test TakeDataAs move semantics
TEST_F(MemorySinkTest, TakeDataAsMove) {
  int width = 100;
  int height = 100;
  int channels = 3;
  auto test_data = CreateTestImageData(width, height, channels);

  auto source =
      MemorySource::CreateTyped(test_data, width, height, channels,
                                DataLayout::kChannelsLast, TileSize(32, 32));

  auto sink = MemorySink::Create();
  sink.Render(source);

  // Move data out
  auto moved_data = sink.TakeDataAs<uint8_t>();

  EXPECT_EQ(moved_data.size(), test_data.size());
  for (size_t i = 0; i < test_data.size(); ++i) {
    EXPECT_EQ(moved_data[i], test_data[i]);
  }
}

}  // namespace
}  // namespace fim

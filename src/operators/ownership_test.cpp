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
 * @file ownership_test.cpp
 * @brief Tests for operator ownership and lifetime management.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains tests to verify that chained operators properly
 * manage their input lifetimes and don't have dangling references.
 */

#include <gtest/gtest.h>

#include "fim/image.h"
#include "fim/operators/crop.h"
#include "fim/operators/downsample.h"
#include "fim/sources/memory_source.h"

namespace fim {
namespace {

/**
 * @brief Test that chained operators don't cause dangling references.
 *
 * This test creates a chain of operators and verifies that the final
 * operator can successfully retrieve tiles even though intermediate
 * temporary objects have been destroyed.
 */
TEST(OwnershipTest, ChainedOperatorsNoRangling) {
  // Create a simple test image
  std::vector<uint8_t> data(100 * 100 * 3, 128);
  ImageInfo dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);

  // Create a chained pipeline using temporaries
  // This used to cause dangling references with the old const& design
  auto pipeline =
      Image<MemorySource>(MemorySource::Create(std::move(data), dims))
          .Crop(10, 10, 80, 80)
          .Downsample(2);

  // Verify we can get dimensions (tests that the chain is valid)
  auto output_dims = pipeline.GetSource().GetDimensions();
  EXPECT_EQ(output_dims.GetWidth(), 40);
  EXPECT_EQ(output_dims.GetHeight(), 40);
  EXPECT_EQ(output_dims.channels, 3);

  // Verify we can actually get tiles (tests that data is accessible)
  auto tile = pipeline.GetSource().GetTile(0, 0, 10, 10);
  EXPECT_EQ(tile.width, 10);
  EXPECT_EQ(tile.height, 10);
  EXPECT_EQ(tile.channels, 3);
}

/**
 * @brief Test that moved sources remain valid in operators.
 *
 * This test verifies that when a source is moved into an operator,
 * the operator maintains ownership and the data remains accessible.
 */
TEST(OwnershipTest, MovedSourcesRemainValid) {
  // Create test data
  std::vector<uint8_t> data(50 * 50 * 1, 255);
  ImageInfo dims(50, 50, 1, PixelType::kUInt8, DataLayout::kChannelsLast);

  // Create a source and move it into a crop operator
  MemorySource source = MemorySource::Create(std::move(data), dims);
  Crop<MemorySource> crop_op(std::move(source), 5, 5, 40, 40);

  // Verify the crop operator can access the data
  auto crop_dims = crop_op.GetDimensions();
  EXPECT_EQ(crop_dims.GetWidth(), 40);
  EXPECT_EQ(crop_dims.GetHeight(), 40);

  auto tile = crop_op.GetTile(0, 0, 10, 10);
  EXPECT_EQ(tile.width, 10);
  EXPECT_EQ(tile.height, 10);
  EXPECT_EQ(tile.GetData()[0], 255);
}

/**
 * @brief Test that multiple chains from the same source work independently.
 *
 * This test verifies that creating multiple processing chains works
 * correctly with the move semantics.
 */
TEST(OwnershipTest, MultipleChainsIndependent) {
  // Create test data
  auto create_source = []() {
    std::vector<uint8_t> data(64 * 64 * 3, 100);
    return MemorySource::Create(
        std::move(data),
        ImageInfo(64, 64, 3, PixelType::kUInt8, DataLayout::kChannelsLast));
  };

  // Create two independent chains
  auto chain1 = Crop<MemorySource>(create_source(), 0, 0, 32, 32);
  auto chain2 = Downsample<MemorySource>(create_source(), 2);

  // Verify both chains work independently
  auto dims1 = chain1.GetDimensions();
  EXPECT_EQ(dims1.GetWidth(), 32);
  EXPECT_EQ(dims1.GetHeight(), 32);

  auto dims2 = chain2.GetDimensions();
  EXPECT_EQ(dims2.GetWidth(), 32);
  EXPECT_EQ(dims2.GetHeight(), 32);

  // Verify both can retrieve tiles
  auto tile1 = chain1.GetTile(0, 0, 10, 10);
  auto tile2 = chain2.GetTile(0, 0, 10, 10);

  EXPECT_EQ(tile1.width, 10);
  EXPECT_EQ(tile2.width, 10);
}

/**
 * @brief Test that complex chains with multiple operations work correctly.
 *
 * This test creates a more complex chain to stress-test the ownership
 * semantics with multiple intermediate operators.
 */
TEST(OwnershipTest, ComplexChainPreservesData) {
  // Create a larger test image with a pattern
  std::vector<uint8_t> data(200 * 200 * 3);
  for (size_t i = 0; i < data.size(); i += 3) {
    data[i] = static_cast<uint8_t>(i % 256);
    data[i + 1] = static_cast<uint8_t>((i + 85) % 256);
    data[i + 2] = static_cast<uint8_t>((i + 170) % 256);
  }

  ImageInfo dims(200, 200, 3, PixelType::kUInt8, DataLayout::kChannelsLast);

  // Create a complex chain
  auto pipeline =
      Image<MemorySource>(MemorySource::Create(std::move(data), dims))
          .Crop(20, 20, 160, 160)
          .Downsample(2)
          .Crop(10, 10, 60, 60);

  // Verify final dimensions
  auto final_dims = pipeline.GetSource().GetDimensions();
  EXPECT_EQ(final_dims.GetWidth(), 60);
  EXPECT_EQ(final_dims.GetHeight(), 60);
  EXPECT_EQ(final_dims.channels, 3);

  // Verify we can get tiles from the final stage
  auto tile = pipeline.GetSource().GetTile(25, 25, 10, 10);
  EXPECT_EQ(tile.width, 10);
  EXPECT_EQ(tile.height, 10);
  EXPECT_EQ(tile.channels, 3);

  // Verify the data is accessible (not garbage from dangling reference)
  EXPECT_LT(tile.GetData()[0], 256);
}

}  // namespace
}  // namespace fim

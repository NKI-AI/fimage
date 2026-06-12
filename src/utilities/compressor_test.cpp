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
 * @file compressor_test.cpp
 * @brief Tests for compression utilities.
 * @author Jonas Teuwen
 * @date 2025
 */
#include "fim/utilities/compressor.h"

#include <vector>

#include "fim/utilities/decompressor.h"
#include "gtest/gtest.h"

namespace fim {
namespace {

// Test data
std::vector<uint8_t> CreateTestData(size_t size) {
  std::vector<uint8_t> data(size);
  for (size_t i = 0; i < size; ++i) {
    data[i] = static_cast<uint8_t>(i % 256);
  }
  return data;
}

// Test NoCompressor
TEST(CompressorTest, NoCompressor) {
  auto data = CreateTestData(1024);
  NoCompressor compressor;

  auto result = compressor.Compress(data);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, data);
}

// Test LZ4Compressor
TEST(CompressorTest, LZ4Compressor) {
  auto data = CreateTestData(1024);
  LZ4Compressor compressor;

  auto result = compressor.Compress(data);
  ASSERT_TRUE(result.ok());

  // Compressed data should be smaller (or at least valid)
  EXPECT_GT(result->size(), 0u);
}

// Test ZstdCompressor
TEST(CompressorTest, ZstdCompressor) {
  auto data = CreateTestData(1024);
  ZstdCompressor compressor;

  auto result = compressor.Compress(data);
  ASSERT_TRUE(result.ok());

  // Compressed data should be smaller (or at least valid)
  EXPECT_GT(result->size(), 0u);
}

// Test factory function with kNone
TEST(CompressorTest, FactoryNone) {
  auto data = CreateTestData(1024);

  auto result = CompressData(data, CompressionType::kNone);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, data);
}

// Test factory function with kLZ4
TEST(CompressorTest, FactoryLZ4) {
  auto data = CreateTestData(1024);

  auto result = CompressData(data, CompressionType::kLZ4);
  ASSERT_TRUE(result.ok());
  EXPECT_GT(result->size(), 0u);
}

// Test factory function with kZstd
TEST(CompressorTest, FactoryZstd) {
  auto data = CreateTestData(1024);

  auto result = CompressData(data, CompressionType::kZstd);
  ASSERT_TRUE(result.ok());
  EXPECT_GT(result->size(), 0u);
}

// Test round-trip compression/decompression with LZ4
TEST(CompressorTest, RoundTripLZ4) {
  auto original = CreateTestData(1024);

  auto compressed = CompressData(original, CompressionType::kLZ4);
  ASSERT_TRUE(compressed.ok());

  auto decompressed =
      DecompressData(*compressed, original.size(), CompressionType::kLZ4);
  ASSERT_TRUE(decompressed.ok());

  EXPECT_EQ(*decompressed, original);
}

// Test round-trip compression/decompression with Zstd
TEST(CompressorTest, RoundTripZstd) {
  auto original = CreateTestData(1024);

  auto compressed = CompressData(original, CompressionType::kZstd);
  ASSERT_TRUE(compressed.ok());

  auto decompressed =
      DecompressData(*compressed, original.size(), CompressionType::kZstd);
  ASSERT_TRUE(decompressed.ok());

  EXPECT_EQ(*decompressed, original);
}

// Test empty data compression
TEST(CompressorTest, EmptyDataLZ4) {
  std::vector<uint8_t> empty;

  auto result = CompressData(empty, CompressionType::kLZ4);
  ASSERT_TRUE(result.ok());
}

TEST(CompressorTest, EmptyDataZstd) {
  std::vector<uint8_t> empty;

  auto result = CompressData(empty, CompressionType::kZstd);
  ASSERT_TRUE(result.ok());
}

// Test large data compression
TEST(CompressorTest, LargeDataLZ4) {
  auto data = CreateTestData(1024 * 1024);  // 1MB

  auto result = CompressData(data, CompressionType::kLZ4);
  ASSERT_TRUE(result.ok());
  EXPECT_GT(result->size(), 0u);
}

TEST(CompressorTest, LargeDataZstd) {
  auto data = CreateTestData(1024 * 1024);  // 1MB

  auto result = CompressData(data, CompressionType::kZstd);
  ASSERT_TRUE(result.ok());
  EXPECT_GT(result->size(), 0u);
}

// Test highly compressible data
TEST(CompressorTest, CompressibleDataLZ4) {
  std::vector<uint8_t> data(1024, 42);  // All same value

  auto result = CompressData(data, CompressionType::kLZ4);
  ASSERT_TRUE(result.ok());

  // Should compress very well
  EXPECT_LT(result->size(), data.size());
}

TEST(CompressorTest, CompressibleDataZstd) {
  std::vector<uint8_t> data(1024, 42);  // All same value

  auto result = CompressData(data, CompressionType::kZstd);
  ASSERT_TRUE(result.ok());

  // Should compress very well
  EXPECT_LT(result->size(), data.size());
}

}  // namespace
}  // namespace fim

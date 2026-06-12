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
 * @file decompressor_test.cpp
 * @brief Tests for decompression utilities.
 * @author Jonas Teuwen
 * @date 2025
 */
#include "fim/utilities/decompressor.h"

#include <vector>

#include "fim/utilities/compressor.h"
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

// Test NoDecompressor
TEST(DecompressorTest, NoDecompressor) {
  auto data = CreateTestData(1024);
  NoDecompressor decompressor;

  auto result = decompressor.Decompress(data, data.size());
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, data);
}

// Test LZ4Decompressor with valid data
TEST(DecompressorTest, LZ4Decompressor) {
  auto original = CreateTestData(1024);

  // First compress
  auto compressed = CompressData(original, CompressionType::kLZ4);
  ASSERT_TRUE(compressed.ok());

  // Then decompress
  LZ4Decompressor decompressor;
  auto result = decompressor.Decompress(*compressed, original.size());
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, original);
}

// Test ZstdDecompressor with valid data
TEST(DecompressorTest, ZstdDecompressor) {
  auto original = CreateTestData(1024);

  // First compress
  auto compressed = CompressData(original, CompressionType::kZstd);
  ASSERT_TRUE(compressed.ok());

  // Then decompress
  ZstdDecompressor decompressor;
  auto result = decompressor.Decompress(*compressed, original.size());
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, original);
}

// Test factory function with kNone
TEST(DecompressorTest, FactoryNone) {
  auto data = CreateTestData(1024);

  auto result = DecompressData(data, data.size(), CompressionType::kNone);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, data);
}

// Test factory function with kLZ4
TEST(DecompressorTest, FactoryLZ4) {
  auto original = CreateTestData(1024);
  auto compressed = CompressData(original, CompressionType::kLZ4);
  ASSERT_TRUE(compressed.ok());

  auto result =
      DecompressData(*compressed, original.size(), CompressionType::kLZ4);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, original);
}

// Test factory function with kZstd
TEST(DecompressorTest, FactoryZstd) {
  auto original = CreateTestData(1024);
  auto compressed = CompressData(original, CompressionType::kZstd);
  ASSERT_TRUE(compressed.ok());

  auto result =
      DecompressData(*compressed, original.size(), CompressionType::kZstd);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, original);
}

// Test decompression with invalid LZ4 data
TEST(DecompressorTest, InvalidLZ4Data) {
  std::vector<uint8_t> invalid_data(100, 0xFF);  // Invalid compressed data

  auto result = DecompressData(invalid_data, 1024, CompressionType::kLZ4);
  EXPECT_FALSE(result.ok());
}

// Test decompression with invalid Zstd data
TEST(DecompressorTest, InvalidZstdData) {
  std::vector<uint8_t> invalid_data(100, 0xFF);  // Invalid compressed data

  auto result = DecompressData(invalid_data, 1024, CompressionType::kZstd);
  EXPECT_FALSE(result.ok());
}

// Test decompression with wrong expected size for LZ4
TEST(DecompressorTest, WrongSizeLZ4) {
  auto original = CreateTestData(1024);
  auto compressed = CompressData(original, CompressionType::kLZ4);
  ASSERT_TRUE(compressed.ok());

  // Try to decompress with wrong expected size
  auto result =
      DecompressData(*compressed, 2048, CompressionType::kLZ4);  // Wrong size
  EXPECT_FALSE(result.ok());
}

// Test decompression with wrong expected size for Zstd
TEST(DecompressorTest, WrongSizeZstd) {
  auto original = CreateTestData(1024);
  auto compressed = CompressData(original, CompressionType::kZstd);
  ASSERT_TRUE(compressed.ok());

  // Try to decompress with wrong expected size
  auto result =
      DecompressData(*compressed, 2048, CompressionType::kZstd);  // Wrong size
  EXPECT_FALSE(result.ok());
}

// Test empty data decompression
TEST(DecompressorTest, EmptyDataLZ4) {
  std::vector<uint8_t> empty;
  auto compressed = CompressData(empty, CompressionType::kLZ4);
  ASSERT_TRUE(compressed.ok());

  auto result = DecompressData(*compressed, 0, CompressionType::kLZ4);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->empty());
}

TEST(DecompressorTest, EmptyDataZstd) {
  std::vector<uint8_t> empty;
  auto compressed = CompressData(empty, CompressionType::kZstd);
  ASSERT_TRUE(compressed.ok());

  auto result = DecompressData(*compressed, 0, CompressionType::kZstd);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->empty());
}

// Test large data decompression
TEST(DecompressorTest, LargeDataLZ4) {
  auto original = CreateTestData(1024 * 1024);  // 1MB
  auto compressed = CompressData(original, CompressionType::kLZ4);
  ASSERT_TRUE(compressed.ok());

  auto result =
      DecompressData(*compressed, original.size(), CompressionType::kLZ4);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, original);
}

TEST(DecompressorTest, LargeDataZstd) {
  auto original = CreateTestData(1024 * 1024);  // 1MB
  auto compressed = CompressData(original, CompressionType::kZstd);
  ASSERT_TRUE(compressed.ok());

  auto result =
      DecompressData(*compressed, original.size(), CompressionType::kZstd);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, original);
}

}  // namespace
}  // namespace fim

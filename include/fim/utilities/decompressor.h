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
 * @file decompressor.h
 * @brief Decompression utilities for FImage format using CRTP pattern.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains decompression utilities using CRTP for zero-overhead
 * abstraction. Supports multiple decompression algorithms (None, LZ4, Zstd).
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_DECOMPRESSOR_H_
#define AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_DECOMPRESSOR_H_

#include <cstdint>
#include <vector>

#include "aifocore/status/result.h"
#include "fim/types.h"  // For CompressionType

namespace fim {

/**
 * @brief CRTP base class for decompressors.
 *
 * This class uses the Curiously Recurring Template Pattern to provide
 * compile-time polymorphism without virtual function overhead.
 *
 * @tparam Derived The concrete decompressor implementation
 */
template <typename Derived>
class DecompressorBase {
 public:
  /**
   * @brief Decompresses data using the derived implementation.
   *
   * @param compressed Compressed data to decompress
   * @param expected_size Expected size of decompressed data
   * @return Decompressed data or error status
   */
  aifocore::Result<std::vector<uint8_t>> Decompress(
      const std::vector<uint8_t>& compressed, size_t expected_size) {
    return static_cast<Derived*>(this)->DecompressImpl(compressed,
                                                       expected_size);
  }
};

/**
 * @brief No-op decompressor that returns data as-is.
 */
class NoDecompressor : public DecompressorBase<NoDecompressor> {
 public:
  /**
   * @brief Returns input data without modification.
   *
   * @param compressed Input data
   * @param expected_size Expected size (unused for no-op)
   * @return Copy of input data
   */
  aifocore::Result<std::vector<uint8_t>> DecompressImpl(
      const std::vector<uint8_t>& compressed, size_t expected_size);
};

/**
 * @brief LZ4 decompressor for fast decompression.
 */
class LZ4Decompressor : public DecompressorBase<LZ4Decompressor> {
 public:
  /**
   * @brief Decompresses data using LZ4 algorithm.
   *
   * @param compressed Compressed data
   * @param expected_size Expected size of decompressed data
   * @return Decompressed data or error status
   */
  aifocore::Result<std::vector<uint8_t>> DecompressImpl(
      const std::vector<uint8_t>& compressed, size_t expected_size);
};

/**
 * @brief Zstd decompressor for Zstandard-compressed data.
 */
class ZstdDecompressor : public DecompressorBase<ZstdDecompressor> {
 public:
  /**
   * @brief Decompresses data using Zstandard algorithm.
   *
   * @param compressed Compressed data
   * @param expected_size Expected size of decompressed data
   * @return Decompressed data or error status
   */
  aifocore::Result<std::vector<uint8_t>> DecompressImpl(
      const std::vector<uint8_t>& compressed, size_t expected_size);
};

/**
 * @brief Factory function to decompress data based on compression type.
 *
 * This function dispatches to the appropriate decompressor implementation
 * based on the CompressionType enum value.
 *
 * @param compressed Compressed data to decompress
 * @param expected_size Expected size of decompressed data
 * @param type Compression algorithm used
 * @return Decompressed data or error status
 */
aifocore::Result<std::vector<uint8_t>> DecompressData(
    const std::vector<uint8_t>& compressed, size_t expected_size,
    CompressionType type);

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_DECOMPRESSOR_H_

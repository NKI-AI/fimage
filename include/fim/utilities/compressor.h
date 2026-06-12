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
 * @file compressor.h
 * @brief Compression utilities for FImage format using CRTP pattern.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains compression utilities using CRTP for zero-overhead
 * abstraction. Supports multiple compression algorithms (None, LZ4, Zstd).
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_COMPRESSOR_H_
#define AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_COMPRESSOR_H_

#include <cstdint>
#include <vector>

#include "aifocore/status/result.h"
#include "fim/types.h"  // For CompressionType

namespace fim {

/**
 * @brief CRTP base class for compressors.
 *
 * This class uses the Curiously Recurring Template Pattern to provide
 * compile-time polymorphism without virtual function overhead.
 *
 * @tparam Derived The concrete compressor implementation
 */
template <typename Derived>
class CompressorBase {
 public:
  /**
   * @brief Compresses data using the derived implementation.
   *
   * @param data Input data to compress
   * @return Compressed data or error status
   */
  aifocore::Result<std::vector<uint8_t>> Compress(
      const std::vector<uint8_t>& data) {
    return static_cast<Derived*>(this)->CompressImpl(data);
  }
};

/**
 * @brief No-op compressor that returns data as-is.
 */
class NoCompressor : public CompressorBase<NoCompressor> {
 public:
  /**
   * @brief Returns input data without modification.
   *
   * @param data Input data
   * @return Copy of input data
   */
  aifocore::Result<std::vector<uint8_t>> CompressImpl(
      const std::vector<uint8_t>& data);
};

/**
 * @brief LZ4 compressor for fast compression.
 */
class LZ4Compressor : public CompressorBase<LZ4Compressor> {
 public:
  /**
   * @brief Compresses data using LZ4 algorithm.
   *
   * @param data Input data to compress
   * @return Compressed data or error status
   */
  aifocore::Result<std::vector<uint8_t>> CompressImpl(
      const std::vector<uint8_t>& data);
};

/**
 * @brief Zstd compressor for better compression ratio.
 */
class ZstdCompressor : public CompressorBase<ZstdCompressor> {
 public:
  /**
   * @brief Compresses data using Zstandard algorithm.
   *
   * Uses compression level 3 for balanced speed/ratio.
   *
   * @param data Input data to compress
   * @return Compressed data or error status
   */
  aifocore::Result<std::vector<uint8_t>> CompressImpl(
      const std::vector<uint8_t>& data);
};

/**
 * @brief Factory function to compress data based on compression type.
 *
 * This function dispatches to the appropriate compressor implementation
 * based on the CompressionType enum value.
 *
 * @param data Input data to compress
 * @param type Compression algorithm to use
 * @return Compressed data or error status
 */
aifocore::Result<std::vector<uint8_t>> CompressData(
    const std::vector<uint8_t>& data, CompressionType type);

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_COMPRESSOR_H_

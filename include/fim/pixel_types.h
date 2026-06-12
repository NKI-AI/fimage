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
 * @file pixel_types.h
 * @brief Basic enumerations for the fim image processing library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the fundamental enumeration types used throughout the fim
 * library. It is separate from types.h to avoid circular dependencies.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_PIXEL_TYPES_H_
#define AIFO_FIMAGE_INCLUDE_FIM_PIXEL_TYPES_H_

#include <cstdint>

namespace fim {

/**
 * @brief Specifies the memory layout of multi-channel image data.
 *
 * This enum determines how multi-channel pixel data is arranged in memory.
 * Different layouts have different performance characteristics depending on
 * the operation being performed.
 */
enum class DataLayout : std::uint8_t {
  kChannelsLast,  ///< Interleaved layout: RGBRGBRGB... (HWC format)
  kChannelsFirst  ///< Planar layout: RRR...GGG...BBB... (CHW format)
};

/**
 * @brief Pixel data type enumeration for image formats.
 *
 * Defines the supported pixel data types in the fim library.
 */
enum class PixelType : std::uint8_t {
  kUInt8 = 0,   ///< 8-bit unsigned integer
  kUInt16 = 1,  ///< 16-bit unsigned integer
  kFloat32 = 2  ///< 32-bit floating point
};

/**
 * @brief Compression method enumeration for image formats.
 *
 * Defines supported compression algorithms. Compression is typically applied
 * per-tile to enable parallel processing and random access.
 */
enum class CompressionType : std::uint8_t {
  kNone = 0,  ///< No compression
  kLZ4 = 1,   ///< LZ4 compression (fast)
  kZstd = 2   ///< Zstandard compression (better ratio, still fast)
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_PIXEL_TYPES_H_

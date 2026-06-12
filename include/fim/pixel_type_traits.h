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
 * @file pixel_type_traits.h
 * @brief Type traits and helper functions for pixel data types.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file provides compile-time and runtime utilities for working with
 * different pixel data types in the fim library. It maps between C++ types
 * and the PixelType enum, provides size information, and validates type
 * support.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_PIXEL_TYPE_TRAITS_H_
#define AIFO_FIMAGE_INCLUDE_FIM_PIXEL_TYPE_TRAITS_H_

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "fim/pixel_types.h"

namespace fim {

// Note: GetPixelTypeSize is now defined inline in types.h to avoid circular
// dependency

/**
 * @brief Maps a C++ type to its corresponding PixelType enum value.
 *
 * @tparam T The C++ type to map (uint8_t, uint16_t, or float)
 * @return Corresponding PixelType enum value
 */
template <typename T>
constexpr PixelType GetPixelType() {
  if constexpr (std::is_same_v<T, uint8_t>) {
    return PixelType::kUInt8;
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    return PixelType::kUInt16;
  } else if constexpr (std::is_same_v<T, float>) {
    return PixelType::kFloat32;
  } else {
    static_assert(sizeof(T) == 0, "Unsupported pixel type");
  }
}

/**
 * @brief Checks if a C++ type is supported as a pixel type.
 *
 * Supported types: uint8_t, uint16_t, float
 *
 * @tparam T The C++ type to check
 * @return true if supported, false otherwise
 */
template <typename T>
constexpr bool IsSupportedPixelType() {
  return std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
         std::is_same_v<T, float>;
}

/**
 * @brief Gets a human-readable name for a pixel type.
 *
 * @param type PixelType enum value
 * @return String name of the pixel type
 */
inline std::string GetPixelTypeName(PixelType type) {
  switch (type) {
    case PixelType::kUInt8:
      return "uint8";
    case PixelType::kUInt16:
      return "uint16";
    case PixelType::kFloat32:
      return "float32";
    default:
      return "unknown";
  }
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_PIXEL_TYPE_TRAITS_H_

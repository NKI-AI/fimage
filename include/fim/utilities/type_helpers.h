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
 * @file type_helpers.h
 * @brief Type utilities and dispatch helpers for multi-type support.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file provides utilities for working with different pixel types,
 * including dispatch helpers and conversion utilities.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_TYPE_HELPERS_H_
#define AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_TYPE_HELPERS_H_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "fim/pixel_types.h"

namespace fim {

/**
 * @brief Maps a C++ type to its corresponding PixelType enum value at runtime.
 *
 * @tparam T The C++ type (uint8_t, uint16_t, or float)
 * @return Corresponding PixelType enum value
 */
template <typename T>
constexpr PixelType ToPixelType() {
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
 * @brief Dispatches a callable based on the runtime PixelType.
 *
 * This function allows you to execute type-specific code without writing
 * switch statements everywhere. The callable should be a template or
 * overloaded function that accepts different types.
 *
 * @tparam Callable Callable object or function template
 * @param pixel_type The runtime pixel type to dispatch on
 * @param callable The function or callable to invoke
 * @param args Additional arguments to forward to the callable
 * @return Result of invoking the callable with the appropriate type
 *
 * Example:
 * @code
 *   auto result = DispatchOnPixelType(
 *       pixel_type,
 *       [](auto type_tag, int width, int height) {
 *         using T = typename decltype(type_tag)::type;
 *         return std::vector<T>(width * height);
 *       },
 *       width, height);
 * @endcode
 */
template <typename Callable, typename... Args>
auto DispatchOnPixelType(PixelType pixel_type, Callable&& callable,
                         Args&&... args) {
  switch (pixel_type) {
    case PixelType::kUInt8:
      return std::forward<Callable>(callable)(
          std::integral_constant<PixelType, PixelType::kUInt8>{},
          std::forward<Args>(args)...);
    case PixelType::kUInt16:
      return std::forward<Callable>(callable)(
          std::integral_constant<PixelType, PixelType::kUInt16>{},
          std::forward<Args>(args)...);
    case PixelType::kFloat32:
      return std::forward<Callable>(callable)(
          std::integral_constant<PixelType, PixelType::kFloat32>{},
          std::forward<Args>(args)...);
    default:
      throw std::invalid_argument("Unsupported pixel type for dispatch");
  }
}

/**
 * @brief Maps PixelType enum to corresponding C++ type via template argument.
 *
 * This template struct provides a type alias for the C++ type corresponding
 * to each PixelType enum value.
 *
 * Example:
 * @code
 *   using DataType = typename PixelTypeToType<PixelType::kUInt16>::type;
 *   // DataType is uint16_t
 * @endcode
 */
template <PixelType PT>
struct PixelTypeToType;

template <>
struct PixelTypeToType<PixelType::kUInt8> {
  using type = uint8_t;
};

template <>
struct PixelTypeToType<PixelType::kUInt16> {
  using type = uint16_t;
};

template <>
struct PixelTypeToType<PixelType::kFloat32> {
  using type = float;
};

/**
 * @brief Helper type alias for PixelTypeToType.
 */
template <PixelType PT>
using PixelTypeToType_t = typename PixelTypeToType<PT>::type;

/**
 * @brief Converts numpy dtype string to PixelType enum.
 *
 * @param dtype_str NumPy dtype string (e.g., "uint8", "uint16", "float32")
 * @return Corresponding PixelType enum value
 * @throw std::invalid_argument if dtype is not supported
 */
inline PixelType NumpyDtypeToPixelType(const std::string& dtype_str) {
  if (dtype_str == "uint8" || dtype_str == "u1" || dtype_str == "|u1") {
    return PixelType::kUInt8;
  } else if (dtype_str == "uint16" || dtype_str == "<u2" ||
             dtype_str == ">u2" || dtype_str == "u2") {
    return PixelType::kUInt16;
  } else if (dtype_str == "float32" || dtype_str == "<f4" ||
             dtype_str == ">f4" || dtype_str == "f4") {
    return PixelType::kFloat32;
  } else {
    throw std::invalid_argument("Unsupported numpy dtype: " + dtype_str);
  }
}

/**
 * @brief Converts PixelType enum to numpy dtype string.
 *
 * @param pixel_type PixelType enum value
 * @return NumPy dtype string
 */
inline std::string PixelTypeToNumpyDtype(PixelType pixel_type) {
  switch (pixel_type) {
    case PixelType::kUInt8:
      return "uint8";
    case PixelType::kUInt16:
      return "uint16";
    case PixelType::kFloat32:
      return "float32";
    default:
      throw std::invalid_argument("Unsupported pixel type for numpy");
  }
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_TYPE_HELPERS_H_

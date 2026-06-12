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
 * @file layout_utils.h
 * @brief Layout utilities for indexing and conversion between DataLayout modes.
 * @author Jonas Teuwen
 * @date 2025
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_LAYOUT_UTILS_H_
#define AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_LAYOUT_UTILS_H_

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "fim/types.h"

namespace fim::layout_utils {

/**
 * @brief Compute linear index for (x,y,c) in HWC (channels-last) layout.
 */
inline size_t IndexChannelsLast(int width, int channels, int x, int y, int c) {
  return (static_cast<size_t>(y) * static_cast<size_t>(width) +
          static_cast<size_t>(x)) *
             static_cast<size_t>(channels) +
         static_cast<size_t>(c);
}

/**
 * @brief Compute linear index for (x,y,c) in CHW (channels-first) layout.
 */
inline size_t IndexChannelsFirst(int width, int height, int x, int y, int c) {
  return (static_cast<size_t>(c) * static_cast<size_t>(height) +
          static_cast<size_t>(y)) *
             static_cast<size_t>(width) +
         static_cast<size_t>(x);
}

/**
 * @brief Compute linear index for (x,y,c) in the given DataLayout.
 */
inline size_t Index(DataLayout layout, int width, int height, int channels,
                    int x, int y, int c) {
  if (layout == DataLayout::kChannelsLast) {
    return IndexChannelsLast(width, channels, x, y, c);
  }
  return IndexChannelsFirst(width, height, x, y, c);
}

/**
 * @brief Convert a typed HWC/CHW buffer between layouts.
 *
 * The returned vector has the same element count (w*h*c) but reordered to the
 * requested layout.
 */
template <typename T>
std::vector<T> ConvertBufferDataLayout(const std::vector<T>& src, int width,
                                       int height, int channels,
                                       DataLayout src_layout,
                                       DataLayout dst_layout) {
  if (src_layout == dst_layout || channels <= 1) {
    return src;
  }

  if (width < 0 || height < 0 || channels < 0) {
    throw std::invalid_argument("Invalid dimensions for layout conversion");
  }

  const size_t expected = static_cast<size_t>(width) *
                          static_cast<size_t>(height) *
                          static_cast<size_t>(channels);
  if (src.size() != expected) {
    throw std::invalid_argument("ConvertBufferDataLayout: input size mismatch");
  }

  std::vector<T> dst(expected);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      for (int c = 0; c < channels; ++c) {
        const size_t src_idx =
            Index(src_layout, width, height, channels, x, y, c);
        const size_t dst_idx =
            Index(dst_layout, width, height, channels, x, y, c);
        dst[dst_idx] = src[src_idx];
      }
    }
  }
  return dst;
}

/**
 * @brief Convert a tile between layouts (preserves pixel_type, position, and
 * padding metadata).
 */
inline Tile ConvertTileDataLayout(const Tile& src, DataLayout dst_layout) {
  if (src.layout == dst_layout || src.channels <= 1) {
    return src;
  }

  Tile dst(src.x, src.y, src.width, src.height, src.channels, dst_layout,
           src.pixel_type);
  dst.expected_width = src.expected_width;
  dst.expected_height = src.expected_height;
  dst.needs_padding = src.needs_padding;

  std::visit(
      [&](const auto& src_vec) {
        using T = typename std::decay_t<decltype(src_vec)>::value_type;
        auto& dst_vec = dst.GetDataAsMut<T>();
        if (src_vec.size() != dst_vec.size()) {
          throw std::invalid_argument("ConvertTileDataLayout: size mismatch");
        }
        const int w = src.width;
        const int h = src.height;
        const int ch = src.channels;
        for (int y = 0; y < h; ++y) {
          for (int x = 0; x < w; ++x) {
            for (int c = 0; c < ch; ++c) {
              const size_t src_idx = Index(src.layout, w, h, ch, x, y, c);
              const size_t dst_idx = Index(dst_layout, w, h, ch, x, y, c);
              dst_vec[dst_idx] = src_vec[src_idx];
            }
          }
        }
      },
      src.GetVariantData());

  return dst;
}

}  // namespace fim::layout_utils

#endif  // AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_LAYOUT_UTILS_H_

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
 * @file tile_helpers.h
 * @brief Shared helper functions for tile extraction and boundary handling.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file provides reusable utilities for clamping tile requests to image
 * bounds, handling padding, and copying tile data. These functions eliminate
 * code duplication across different image source implementations.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_TILE_HELPERS_H_
#define AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_TILE_HELPERS_H_

#include <algorithm>
#include <vector>

#include "fim/types.h"

namespace fim {
namespace tile_helpers {

/**
 * @brief Result of clamping a tile request to image bounds.
 *
 * Contains the actual coordinates and dimensions after clamping, as well as
 * whether padding is needed to match the requested dimensions.
 */
struct ClampedTileRequest {
  int actual_x;        ///< Clamped X coordinate (>= 0)
  int actual_y;        ///< Clamped Y coordinate (>= 0)
  int actual_width;    ///< Clamped width (may be smaller than requested)
  int actual_height;   ///< Clamped height (may be smaller than requested)
  bool needs_padding;  ///< Whether the actual size differs from requested
  bool is_empty;       ///< Whether the tile is completely outside image bounds
};

/**
 * @brief Clamps a tile request to image boundaries.
 *
 * This function handles requests that may be partially or completely outside
 * the image bounds. It computes the actual valid region that can be read from
 * the source image.
 *
 * @param x Requested X coordinate
 * @param y Requested Y coordinate
 * @param req_width Requested tile width
 * @param req_height Requested tile height
 * @param dims Image dimensions
 * @return ClampedTileRequest with clamped coordinates and padding info
 */
inline ClampedTileRequest ClampTileRequest(int x, int y, int req_width,
                                           int req_height,
                                           const ImageInfo& dims) {
  ClampedTileRequest result;

  // Clamp start coordinates to valid image region
  result.actual_x = std::max(0, x);
  result.actual_y = std::max(0, y);

  // Calculate requested end coordinates
  int end_x = x + req_width;
  int end_y = y + req_height;

  // Clamp end coordinates to image bounds
  int actual_end_x = std::min(end_x, dims.GetWidth());
  int actual_end_y = std::min(end_y, dims.GetHeight());

  // Calculate actual valid data dimensions
  result.actual_width = std::max(0, actual_end_x - result.actual_x);
  result.actual_height = std::max(0, actual_end_y - result.actual_y);

  // Check if tile is completely outside bounds or has different dimensions
  result.is_empty = (result.actual_width <= 0 || result.actual_height <= 0);
  result.needs_padding =
      (result.actual_width != req_width || result.actual_height != req_height);

  return result;
}

/**
 * @brief Creates a tile with appropriate padding information.
 *
 * Based on the clamped request, creates either a regular tile or one with
 * padding metadata, depending on whether the requested size differs from the
 * actual size.
 *
 * @param x Original X coordinate
 * @param y Original Y coordinate
 * @param actual_width Actual width after clamping
 * @param actual_height Actual height after clamping
 * @param req_width Requested width
 * @param req_height Requested height
 * @param dims Image metadata (channels, pixel type, layout)
 * @return Tile with appropriate dimensions and padding info
 */
inline Tile CreateTileWithPadding(int x, int y, int actual_width,
                                  int actual_height, int req_width,
                                  int req_height, const ImageInfo& dims) {
  if (actual_width != req_width || actual_height != req_height) {
    // Needs padding metadata
    return Tile(x, y, actual_width, actual_height, dims.channels, req_width,
                req_height, dims.layout, dims.pixel_type);
  } else {
    // No padding needed
    return Tile(x, y, actual_width, actual_height, dims.channels, dims.layout,
                dims.pixel_type);
  }
}

/**
 * @brief Copies tile data from source buffer using row-wise copy.
 *
 * This template function copies a rectangular region from a source image
 * buffer into a tile. It handles arbitrary pixel types through templates
 * and copies row-by-row for better cache performance.
 *
 * @tparam T The pixel type (uint8_t, uint16_t, or float)
 * @param src_data Source image data buffer
 * @param tile Destination tile (must already be allocated)
 * @param actual_x X offset in source image
 * @param actual_y Y offset in source image
 * @param src_dims Source image dimensions
 */
template <typename T>
void CopyTileRows(const std::vector<T>& src_data, Tile& tile, int actual_x,
                  int actual_y, const ImageInfo& src_dims) {
  auto& tile_data = tile.GetDataAsMut<T>();

  if (src_dims.layout == DataLayout::kChannelsLast) {
    // Copy data from source buffer to tile row-by-row for better cache
    // performance (HWC).
    for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
      int src_y = actual_y + tile_y;
      size_t src_row_start = (static_cast<size_t>(src_y) *
                                  static_cast<size_t>(src_dims.GetWidth()) +
                              static_cast<size_t>(actual_x)) *
                             static_cast<size_t>(src_dims.channels);
      size_t dst_row_start = static_cast<size_t>(tile_y) *
                             static_cast<size_t>(tile.width) *
                             static_cast<size_t>(src_dims.channels);
      std::copy_n(&src_data[src_row_start],
                  static_cast<size_t>(tile.width) *
                      static_cast<size_t>(src_dims.channels),
                  &tile_data[dst_row_start]);
    }
    return;
  }

  // Channels-first (CHW): copy each channel plane row-by-row.
  const int src_w = src_dims.GetWidth();
  const int src_h = src_dims.GetHeight();
  for (int c = 0; c < src_dims.channels; ++c) {
    for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
      int src_y = actual_y + tile_y;
      size_t src_row_start =
          (static_cast<size_t>(c) * static_cast<size_t>(src_h) +
           static_cast<size_t>(src_y)) *
              static_cast<size_t>(src_w) +
          static_cast<size_t>(actual_x);
      size_t dst_row_start =
          (static_cast<size_t>(c) * static_cast<size_t>(tile.height) +
           static_cast<size_t>(tile_y)) *
          static_cast<size_t>(tile.width);
      std::copy_n(&src_data[src_row_start], static_cast<size_t>(tile.width),
                  &tile_data[dst_row_start]);
    }
  }
}

}  // namespace tile_helpers
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_TILE_HELPERS_H_

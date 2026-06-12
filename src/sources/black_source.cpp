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
 * @file black_source.cpp
 * @brief Implementation of BlackSource class.
 * @author Jonas Teuwen
 * @date 2025
 */

#include "fim/sources/black_source.h"

#include <algorithm>
#include <stdexcept>

#include "fim/utilities/tile_helpers.h"

namespace fim {

BlackSource BlackSource::Create(ImageInfo info, TileSize tile_size) {
  if (info.GetWidth() <= 0 || info.GetHeight() <= 0) {
    throw std::invalid_argument("Image dimensions must be positive");
  }
  if (info.channels <= 0) {
    throw std::invalid_argument("Number of channels must be positive");
  }
  return BlackSource(info, tile_size);
}

BlackSource::BlackSource(ImageInfo info, TileSize tile_size)
    : info_(info), tile_size_(tile_size) {}

ImageInfo BlackSource::GetDimensions() const {
  return info_;
}

int BlackSource::GetChannels() const {
  return info_.channels;
}

DataLayout BlackSource::GetMemoryLayout() const {
  return info_.layout;
}

TileSize BlackSource::GetIdealTileSize() const {
  return tile_size_;
}

Tile BlackSource::GetTile(int x, int y, int width, int height) const {
  // Use shared helper to clamp tile request to image bounds
  auto request = tile_helpers::ClampTileRequest(x, y, width, height, info_);

  // Handle case where tile is completely outside image bounds
  if (request.is_empty) {
    return Tile(x, y, 0, 0, info_.channels, info_.layout, info_.pixel_type);
  }

  // Create tile with appropriate padding metadata
  // tile.data is already zero-initialized by std::vector<T> constructor
  return tile_helpers::CreateTileWithPadding(
      x, y, request.actual_width, request.actual_height, width, height, info_);
}

}  // namespace fim

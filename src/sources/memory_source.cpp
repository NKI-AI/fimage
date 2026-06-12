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
 * @file memory_source.cpp
 * @brief Implementation of memory-based image source for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the implementation of the MemorySource class which
 * provides efficient tile-based access to image data stored in memory.
 */
#include "fim/sources/memory_source.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "fim/utilities/tile_helpers.h"

namespace fim {

MemorySource MemorySource::Create(std::vector<uint8_t> image_data,
                                  ImageInfo dimensions, TileSize tile_size) {
  // Convert byte array to appropriate typed variant based on pixel_type
  TileData variant_data;
  size_t num_elements = static_cast<size_t>(dimensions.GetWidth()) *
                        static_cast<size_t>(dimensions.GetHeight()) *
                        static_cast<size_t>(dimensions.channels);

  switch (dimensions.pixel_type) {
    case PixelType::kUInt8: {
      // Validate buffer size for uint8
      if (image_data.size() < num_elements) {
        throw std::invalid_argument(
            "Buffer too small for kUInt8: need " +
            std::to_string(num_elements) + " bytes, got " +
            std::to_string(image_data.size()) + " bytes");
      }
      variant_data = std::move(image_data);
      break;
    }
    case PixelType::kUInt16: {
      size_t required_bytes = num_elements * sizeof(uint16_t);
      if (image_data.size() < required_bytes) {
        throw std::invalid_argument(
            "Buffer too small for kUInt16: need " +
            std::to_string(required_bytes) + " bytes, got " +
            std::to_string(image_data.size()) + " bytes");
      }
      std::vector<uint16_t> typed_data(num_elements);
      std::memcpy(typed_data.data(), image_data.data(), required_bytes);
      variant_data = std::move(typed_data);
      break;
    }
    case PixelType::kFloat32: {
      size_t required_bytes = num_elements * sizeof(float);
      if (image_data.size() < required_bytes) {
        throw std::invalid_argument(
            "Buffer too small for kFloat32: need " +
            std::to_string(required_bytes) + " bytes, got " +
            std::to_string(image_data.size()) + " bytes");
      }
      std::vector<float> typed_data(num_elements);
      std::memcpy(typed_data.data(), image_data.data(), required_bytes);
      variant_data = std::move(typed_data);
      break;
    }
    default:
      throw std::invalid_argument("Unsupported pixel type");
  }

  return MemorySource(std::move(variant_data), dimensions, tile_size);
}

MemorySource MemorySource::Create(TileData image_data, ImageInfo dimensions,
                                  TileSize tile_size) {
  return MemorySource(std::move(image_data), dimensions, tile_size);
}

MemorySource::MemorySource(TileData image_data, ImageInfo dimensions,
                           TileSize tile_size)
    : image_data_(std::move(image_data)),
      dimensions_(dimensions),
      tile_size_(tile_size) {}

MemorySource::MemorySource(MemorySource&& other) noexcept
    : image_data_(std::move(other.image_data_)),
      dimensions_(other.dimensions_),
      tile_size_(other.tile_size_) {}

MemorySource& MemorySource::operator=(MemorySource&& other) noexcept {
  if (this != &other) {
    image_data_ = std::move(other.image_data_);
    dimensions_ = other.dimensions_;
    tile_size_ = other.tile_size_;
  }
  return *this;
}

ImageInfo MemorySource::GetDimensions() const {
  return dimensions_;
}

int MemorySource::GetChannels() const {
  return dimensions_.channels;
}

DataLayout MemorySource::GetMemoryLayout() const {
  return dimensions_.layout;
}

TileSize MemorySource::GetIdealTileSize() const {
  return tile_size_;
}

Tile MemorySource::GetTile(int x, int y, int width, int height) const {
  // Use shared helper to clamp tile request to image bounds
  auto request =
      tile_helpers::ClampTileRequest(x, y, width, height, dimensions_);

  // Handle case where tile is completely outside image bounds
  if (request.is_empty) {
    return Tile(x, y, 0, 0, dimensions_.channels, width, height,
                dimensions_.layout, dimensions_.pixel_type);
  }

  // Create tile with appropriate padding metadata
  Tile tile = tile_helpers::CreateTileWithPadding(x, y, request.actual_width,
                                                  request.actual_height, width,
                                                  height, dimensions_);

  // Use visitor to copy data from variant storage based on actual type
  std::visit(
      [&](const auto& typed_data) {
        tile_helpers::CopyTileRows(typed_data, tile, request.actual_x,
                                   request.actual_y, dimensions_);
      },
      image_data_);

  return tile;
}

size_t MemorySource::GetMemoryUsage() const {
  return std::visit(
      [](const auto& vec) -> size_t {
        return vec.size() *
               sizeof(typename std::decay_t<decltype(vec)>::value_type);
      },
      image_data_);
}

}  // namespace fim


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
 * @file lodepng_png_source.cpp
 * @brief Implementation of PNG image source using lodepng for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the implementation of the PngSource class which provides
 * PNG image reading capabilities using the lodepng library. It handles lazy
 * loading of PNG data and provides tile-based access to the image content.
 */
#include "fim/sources/lodepng_png_source.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

#include "aifocore/platform/portability.h"
#include "fim/utilities/tile_helpers.h"
#include "lodepng/lodepng.h"

namespace fim {

namespace {

std::vector<unsigned char> LoadFileToBuffer(const fs::path& filename) {
  FILE* file = aifocore::portable_fopen(filename, "rb");  // NOLINT
  if (file == nullptr) {
    throw std::runtime_error("Failed to open PNG file: " + filename.string());
  }
  const int64_t size = aifocore::portable_filesize(file);
  if (size < 0) {
    aifocore::portable_fclose(file);
    throw std::runtime_error("Failed to stat PNG file: " + filename.string());
  }
  std::vector<unsigned char> buffer(static_cast<size_t>(size));
  const size_t bytes_read =
      aifocore::portable_fread(buffer.data(), buffer.size(), file);
  aifocore::portable_fclose(file);
  if (bytes_read != buffer.size()) {
    throw std::runtime_error("Failed to read PNG file: " + filename.string());
  }
  return buffer;
}

}  // namespace

PngSource PngSource::Create(const fs::path& filename) {
  // Validate file existence before construction
  if (!std::filesystem::exists(filename)) {
    throw std::runtime_error("PNG file does not exist: " + filename.string());
  }
  return PngSource(filename);
}

PngSource::PngSource(const fs::path& filename)
    : filename_(filename), data_loaded_(false) {
  // Validate file existence early for immediate feedback
  if (!std::filesystem::exists(filename)) {
    throw std::runtime_error("PNG file does not exist: " + filename.string());
  }
  // Lazy materialization: Don't load data in constructor
  // Data will be loaded on first access via GetDimensions() or GetTile()
}

PngSource::PngSource(PngSource&& other) noexcept
    : filename_(std::move(other.filename_)),
      image_data_(std::move(other.image_data_)),
      dimensions_(other.dimensions_),
      data_loaded_(other.data_loaded_) {
  other.data_loaded_ = false;
}

PngSource& PngSource::operator=(PngSource&& other) noexcept {
  if (this != &other) {
    filename_ = std::move(other.filename_);
    image_data_ = std::move(other.image_data_);
    dimensions_ = other.dimensions_;
    data_loaded_ = other.data_loaded_;
    other.data_loaded_ = false;
  }
  return *this;
}

ImageInfo PngSource::GetDimensions() const {
  if (!data_loaded_) {
    LoadData();
  }
  return dimensions_;
}

TileSize PngSource::GetIdealTileSize() const {
  if (!data_loaded_) {
    LoadData();
  }
  // PNG is not tiled, so return the full image size as ideal tile
  return TileSize(dimensions_.GetWidth(), dimensions_.GetHeight());
}

void PngSource::LoadData() const {
  if (data_loaded_) {
    return;
  }

  // First inspect the PNG to get its actual color type
  LodePNGState state;
  lodepng_state_init(&state);

  // Load file into memory for inspection
  std::vector<unsigned char> file_data = LoadFileToBuffer(filename_);

  unsigned int width, height;
  unsigned int error = lodepng_inspect(&width, &height, &state,
                                       file_data.data(), file_data.size());
  if (error) {
    lodepng_state_cleanup(&state);
    throw std::runtime_error("PNG inspect error: " +
                             std::string(lodepng_error_text(error)));
  }

  // Determine channels from color type
  int channels;
  LodePNGColorType color_type = state.info_png.color.colortype;
  switch (color_type) {
    case LCT_GREY:
      channels = 1;
      break;
    case LCT_RGB:
      channels = 3;
      break;
    case LCT_PALETTE:
      // Palette images will be expanded to RGB
      channels = 3;
      break;
    case LCT_GREY_ALPHA:
      channels = 2;
      break;
    case LCT_RGBA:
      channels = 4;
      break;
    default:
      lodepng_state_cleanup(&state);
      throw std::runtime_error("Unsupported PNG color type: " +
                               std::to_string(color_type));
  }

  lodepng_state_cleanup(&state);

  // Decode with appropriate color type to preserve channels
  LodePNGColorType target_color_type;
  unsigned int bit_depth = 8;

  if (channels == 1) {
    target_color_type = LCT_GREY;
  } else if (channels == 2) {
    target_color_type = LCT_GREY_ALPHA;
  } else if (channels == 3) {
    target_color_type = LCT_RGB;
  } else {  // channels == 4
    target_color_type = LCT_RGBA;
  }

  error = lodepng::decode(image_data_, width, height, file_data,
                          target_color_type, bit_depth);
  if (error) {
    throw std::runtime_error("PNG decode error: " +
                             std::string(lodepng_error_text(error)));
  }

  dimensions_ =
      ImageInfo(static_cast<int>(width), static_cast<int>(height), channels,
                PixelType::kUInt8, DataLayout::kChannelsLast);
  data_loaded_ = true;
}

Tile PngSource::GetTile(int x, int y, int width, int height) const {
  if (!data_loaded_) {
    LoadData();
  }

  // Use shared helper to clamp tile request to image bounds
  auto request =
      tile_helpers::ClampTileRequest(x, y, width, height, dimensions_);

  // Handle case where tile is completely outside image bounds
  if (request.is_empty) {
    return Tile(x, y, 0, 0, dimensions_.channels, width, height);
  }

  // Create tile with appropriate padding metadata
  Tile tile = tile_helpers::CreateTileWithPadding(x, y, request.actual_width,
                                                  request.actual_height, width,
                                                  height, dimensions_);

  // Copy data from full image to tile using helper
  tile_helpers::CopyTileRows(image_data_, tile, request.actual_x,
                             request.actual_y, dimensions_);

  return tile;
}

}  // namespace fim

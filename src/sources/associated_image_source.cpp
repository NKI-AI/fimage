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

#include "fim/sources/associated_image_source.h"

#include <algorithm>
#include <stdexcept>

#include "fim/sources/fastslide_source.h"
#if defined(FIM_WITH_OPENSLIDE)
#include "fim/sources/openslide_source.h"
#endif  // defined(FIM_WITH_OPENSLIDE)

namespace fim {

// ============================================================================
// FastSlideAssociatedImageSource Implementation
// ============================================================================

FastSlideAssociatedImageSource::FastSlideAssociatedImageSource(
    std::shared_ptr<FastSlideSource> source, const std::string& name)
    : source_(std::move(source)),
      name_(name),
      loaded_(false),
      width_(0),
      height_(0) {
  if (!source_) {
    throw std::invalid_argument("FastSlideSource pointer cannot be null");
  }
}

FastSlideAssociatedImageSource FastSlideAssociatedImageSource::Create(
    std::shared_ptr<FastSlideSource> source, const std::string& name) {
  return FastSlideAssociatedImageSource(std::move(source), name);
}

void FastSlideAssociatedImageSource::EnsureLoaded() const {
  if (loaded_) {
    return;
  }

  // Load the associated image
  auto [data, width, height] = source_->ReadAssociatedImage(name_);
  data_ = std::move(data);
  width_ = width;
  height_ = height;
  loaded_ = true;
}

ImageInfo FastSlideAssociatedImageSource::GetDimensions() const {
  EnsureLoaded();
  return ImageInfo(width_, height_, 3, PixelType::kUInt8,
                   DataLayout::kChannelsLast);
}

TileSize FastSlideAssociatedImageSource::GetIdealTileSize() const {
  EnsureLoaded();
  return TileSize(width_, height_);  // Return full image as one tile
}

Tile FastSlideAssociatedImageSource::GetTile(int x, int y, int width,
                                             int height) const {
  EnsureLoaded();

  // Clamp request to image bounds
  x = std::max(0, std::min(x, width_ - 1));
  y = std::max(0, std::min(y, height_ - 1));
  width = std::max(0, std::min(width, width_ - x));
  height = std::max(0, std::min(height, height_ - y));

  // Create tile
  Tile tile(x, y, width, height, 3);
  auto& dest = tile.GetDataMut();

  // Copy requested region from loaded data
  for (int row = 0; row < height; ++row) {
    const int src_y = y + row;
    const uint8_t* src_row = &data_[(src_y * width_ + x) * 3];
    uint8_t* dest_row = &dest[row * width * 3];
    std::copy(src_row, src_row + width * 3, dest_row);
  }

  return tile;
}

// ============================================================================
// OpenSlideAssociatedImageSource Implementation
// ============================================================================

#if defined(FIM_WITH_OPENSLIDE)
OpenSlideAssociatedImageSource::OpenSlideAssociatedImageSource(
    std::shared_ptr<OpenSlideSource> source, const std::string& name)
    : source_(std::move(source)),
      name_(name),
      loaded_(false),
      width_(0),
      height_(0) {
  if (!source_) {
    throw std::invalid_argument("OpenSlideSource pointer cannot be null");
  }
}

OpenSlideAssociatedImageSource OpenSlideAssociatedImageSource::Create(
    std::shared_ptr<OpenSlideSource> source, const std::string& name) {
  return OpenSlideAssociatedImageSource(std::move(source), name);
}

void OpenSlideAssociatedImageSource::EnsureLoaded() const {
  if (loaded_) {
    return;
  }

  // Load the associated image
  auto [data, width, height] = source_->ReadAssociatedImage(name_);
  data_ = std::move(data);
  width_ = width;
  height_ = height;
  loaded_ = true;
}

ImageInfo OpenSlideAssociatedImageSource::GetDimensions() const {
  EnsureLoaded();
  return ImageInfo(width_, height_, 3, PixelType::kUInt8,
                   DataLayout::kChannelsLast);
}

TileSize OpenSlideAssociatedImageSource::GetIdealTileSize() const {
  EnsureLoaded();
  return TileSize(width_, height_);  // Return full image as one tile
}

Tile OpenSlideAssociatedImageSource::GetTile(int x, int y, int width,
                                             int height) const {
  EnsureLoaded();

  // Clamp request to image bounds
  x = std::max(0, std::min(x, width_ - 1));
  y = std::max(0, std::min(y, height_ - 1));
  width = std::max(0, std::min(width, width_ - x));
  height = std::max(0, std::min(height, height_ - y));

  // Create tile
  Tile tile(x, y, width, height, 3);
  auto& dest = tile.GetDataMut();

  // Copy requested region from loaded data
  for (int row = 0; row < height; ++row) {
    const int src_y = y + row;
    const uint8_t* src_row = &data_[(src_y * width_ + x) * 3];
    uint8_t* dest_row = &dest[row * width * 3];
    std::copy(src_row, src_row + width * 3, dest_row);
  }

  return tile;
}
#endif  // defined(FIM_WITH_OPENSLIDE)

}  // namespace fim

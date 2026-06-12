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
 * @file associated_image_source.h
 * @brief Source implementations for lazily loaded associated images.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file provides source classes for associated images from FastSlide
 * and OpenSlide, enabling lazy loading and integration with the fimage
 * pipeline.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SOURCES_ASSOCIATED_IMAGE_SOURCE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SOURCES_ASSOCIATED_IMAGE_SOURCE_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "fim/pipeline.h"
#include "fim/types.h"

namespace fim {

// Forward declarations
class FastSlideSource;
class OpenSlideSource;

/**
 * @brief Lazily loaded associated image source from FastSlide.
 *
 * This class provides a lazy source for associated images (thumbnail, macro,
 * label) from FastSlide readers. The image is only loaded when tiles are
 * requested, not when the source is created.
 */
class FastSlideAssociatedImageSource
    : public SourceBase<FastSlideAssociatedImageSource> {
 public:
  /**
   * @brief Creates an associated image source.
   *
   * @param source Shared pointer to the FastSlideSource
   * @param name Name of the associated image (e.g., "thumbnail")
   * @return FastSlideAssociatedImageSource instance
   * @throw std::runtime_error if image cannot be accessed
   */
  static FastSlideAssociatedImageSource Create(
      std::shared_ptr<FastSlideSource> source, const std::string& name);

  ImageInfo GetDimensions() const;
  TileSize GetIdealTileSize() const;
  Tile GetTile(int x, int y, int width, int height) const;

 private:
  FastSlideAssociatedImageSource(std::shared_ptr<FastSlideSource> source,
                                 const std::string& name);

  void EnsureLoaded() const;

  std::shared_ptr<FastSlideSource> source_;
  std::string name_;
  mutable bool loaded_;
  mutable std::vector<uint8_t> data_;
  mutable int width_;
  mutable int height_;
};

#if defined(FIM_WITH_OPENSLIDE)
/**
 * @brief Lazily loaded associated image source from OpenSlide.
 *
 * This class provides a lazy source for associated images from OpenSlide
 * readers. The image is only loaded when tiles are requested.
 */
class OpenSlideAssociatedImageSource
    : public SourceBase<OpenSlideAssociatedImageSource> {
 public:
  /**
   * @brief Creates an associated image source.
   *
   * @param source Shared pointer to the OpenSlideSource
   * @param name Name of the associated image (e.g., "thumbnail")
   * @return OpenSlideAssociatedImageSource instance
   * @throw std::runtime_error if image cannot be accessed
   */
  static OpenSlideAssociatedImageSource Create(
      std::shared_ptr<OpenSlideSource> source, const std::string& name);

  ImageInfo GetDimensions() const;
  TileSize GetIdealTileSize() const;
  Tile GetTile(int x, int y, int width, int height) const;

 private:
  OpenSlideAssociatedImageSource(std::shared_ptr<OpenSlideSource> source,
                                 const std::string& name);

  void EnsureLoaded() const;

  std::shared_ptr<OpenSlideSource> source_;
  std::string name_;
  mutable bool loaded_;
  mutable std::vector<uint8_t> data_;
  mutable int width_;
  mutable int height_;
};
#endif  // defined(FIM_WITH_OPENSLIDE)

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SOURCES_ASSOCIATED_IMAGE_SOURCE_H_

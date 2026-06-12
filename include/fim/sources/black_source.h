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
 * @file black_source.h
 * @brief Black canvas image source implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the BlackSource class which provides a lazy black canvas
 * that generates zero-valued pixels on demand without allocating the full image
 * in memory. This is useful for creating composite images with paste
 * operations.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SOURCES_BLACK_SOURCE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SOURCES_BLACK_SOURCE_H_

#include "fim/pipeline.h"
#include "fim/types.h"

namespace fim {

/**
 * @brief Black canvas image source that generates zero-valued pixels on demand.
 *
 * This class provides a source implementation for a black (zero-valued) canvas
 * of specified dimensions and channel count. It generates black pixels
 * on-the-fly for each requested tile without allocating memory for the full
 * image, making it very memory-efficient for large canvases.
 *
 * The class follows the CRTP pattern by inheriting from SourceBase and
 * provides the required interface methods for image sources.
 *
 * Typical use case is as a base layer for paste operations where multiple
 * images are composited onto a black background.
 *
 * @note Use Create() factory method to instantiate.
 */
class BlackSource : public SourceBase<BlackSource> {
 public:
  /**
   * @brief Creates a black canvas source (factory method).
   *
   * This is the recommended way to create a BlackSource instance.
   *
   * @param info Image info including width, height, channels, pixel type, and
   * layout
   * @param tile_size Preferred tile size for efficient access
   * @return BlackSource instance
   */
  static BlackSource Create(ImageInfo info,
                            TileSize tile_size = TileSize(256, 256));

  /**
   * @brief Move constructor.
   *
   * @param other BlackSource to move from
   */
  BlackSource(BlackSource&& other) noexcept = default;

  /**
   * @brief Move assignment operator.
   *
   * @param other BlackSource to move from
   * @return Reference to this instance
   */
  BlackSource& operator=(BlackSource&& other) noexcept = default;

  /**
   * @brief Deleted copy constructor.
   */
  BlackSource(const BlackSource&) = delete;

  /**
   * @brief Deleted copy assignment operator.
   */
  BlackSource& operator=(const BlackSource&) = delete;

  /**
   * @brief Default destructor.
   */
  ~BlackSource() = default;

  /**
   * @brief Gets the dimensions of the black canvas.
   *
   * @return ImageInfo containing the canvas dimensions
   */
  ImageInfo GetDimensions() const;

  /**
   * @brief Gets the number of channels in the canvas.
   *
   * @return Number of color channels
   */
  int GetChannels() const;

  /**
   * @brief Gets the memory layout of the image data.
   *
   * @return DataLayout specifying the memory organization (always ChannelsLast)
   */
  DataLayout GetMemoryLayout() const;

  /**
   * @brief Gets the ideal tile size for processing.
   *
   * @return TileSize containing the ideal tile dimensions
   */
  TileSize GetIdealTileSize() const;

  /**
   * @brief Gets a tile filled with black (zero-valued) pixels.
   *
   * This method generates a tile of the requested size filled with zeros.
   * No actual image data is stored; zeros are generated on-the-fly.
   *
   * @param x X coordinate of the top-left corner of the tile
   * @param y Y coordinate of the top-left corner of the tile
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile containing black (zero) pixels
   */
  Tile GetTile(int x, int y, int width, int height) const;

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param info Image info including width, height, channels, pixel type, and
   * layout
   * @param tile_size Preferred tile size for efficient access
   */
  BlackSource(ImageInfo info, TileSize tile_size);

  ImageInfo info_;      ///< Canvas dimensions and metadata
  TileSize tile_size_;  ///< Preferred tile size
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SOURCES_BLACK_SOURCE_H_

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
 * @file memory_source.h
 * @brief Memory-based image source implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the MemorySource class which provides image reading
 * capabilities from in-memory data for the fim image processing pipeline.
 * It's particularly useful for pyramid generation where intermediate levels
 * are kept in memory for efficient downsampling.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SOURCES_MEMORY_SOURCE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SOURCES_MEMORY_SOURCE_H_

#include <algorithm>
#include <vector>

#include "fim/pipeline.h"
#include "fim/types.h"

namespace fim {

/**
 * @brief Memory-based image source for in-memory image data.
 *
 * This class provides a source implementation for image data stored in memory.
 * It's designed for efficient tile-based access to image data that's already
 * loaded in memory, making it ideal for pyramid generation and processing
 * intermediate results.
 *
 * The class follows the CRTP pattern by inheriting from SourceBase and
 * provides the required interface methods for image sources.
 *
 * @note Use Create() factory method to instantiate.
 */
class MemorySource : public SourceBase<MemorySource> {
 public:
  /**
   * @brief Creates a memory source from existing image data (factory method).
   *
   * This is the only way to create a MemorySource instance.
   *
   * @param image_data Vector containing the image data in row-major order (as
   * bytes)
   * @param dimensions Image dimensions including width, height, channels, and
   * pixel type
   * @param tile_size Preferred tile size for efficient access
   * @return MemorySource instance
   *
   * @note Future: This will be migrated to return absl::StatusOr<MemorySource>
   * to avoid exceptions.
   */
  static MemorySource Create(std::vector<uint8_t> image_data,
                             ImageInfo dimensions,
                             TileSize tile_size = TileSize(256, 256));

  /**
   * @brief Creates a memory source from typed image data (factory method).
   *
   * This overload accepts TileData directly, avoiding costly memcpy operations
   * for non-uint8 pixel types. Use this when you already have typed data
   * (e.g., from AssembleImageData or AssembleDownsampledData).
   *
   * @param image_data TileData variant containing the image data in row-major
   * order
   * @param dimensions Image dimensions including width, height, channels, and
   * pixel type
   * @param tile_size Preferred tile size for efficient access
   * @return MemorySource instance
   */
  static MemorySource Create(TileData image_data, ImageInfo dimensions,
                             TileSize tile_size = TileSize(256, 256));

  /**
   * @brief Creates a memory source from typed image data (factory method).
   *
   * This is a templated factory method that accepts data in the native type.
   *
   * @tparam T The pixel type (uint8_t, uint16_t, or float)
   * @param image_data Vector containing the image data in row-major order
   * @param width Image width in pixels
   * @param height Image height in pixels
   * @param channels Number of channels
   * @param layout Data layout (default: kChannelsLast)
   * @param tile_size Preferred tile size for efficient access
   * @return MemorySource instance
   */
  template <typename T>
  static MemorySource CreateTyped(std::vector<T> image_data, int width,
                                  int height, int channels,
                                  DataLayout layout = DataLayout::kChannelsLast,
                                  TileSize tile_size = TileSize(256, 256));

  /**
   * @brief Move constructor.
   *
   * @param other MemorySource to move from
   */
  MemorySource(MemorySource&& other) noexcept;

  /**
   * @brief Move assignment operator.
   *
   * @param other MemorySource to move from
   * @return Reference to this instance
   */
  MemorySource& operator=(MemorySource&& other) noexcept;

  /**
   * @brief Deleted copy constructor.
   */
  MemorySource(const MemorySource&) = delete;

  /**
   * @brief Deleted copy assignment operator.
   */
  MemorySource& operator=(const MemorySource&) = delete;

  /**
   * @brief Default destructor.
   */
  ~MemorySource() = default;

  /**
   * @brief Gets the dimensions of the image.
   *
   * @return ImageInfo containing the image dimensions
   */
  ImageInfo GetDimensions() const;

  /**
   * @brief Gets the number of channels in the image.
   *
   * @return Number of color channels
   */
  int GetChannels() const;

  /**
   * @brief Gets the memory layout of the image data.
   *
   * @return DataLayout specifying the memory organization
   */
  DataLayout GetMemoryLayout() const;

  /**
   * @brief Gets the ideal tile size for processing.
   *
   * @return TileSize containing the ideal tile dimensions
   */
  TileSize GetIdealTileSize() const;

  /**
   * @brief Gets a tile from the image data.
   *
   * @param x X coordinate of the top-left corner of the tile
   * @param y Y coordinate of the top-left corner of the tile
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile containing the requested image data
   */
  Tile GetTile(int x, int y, int width, int height) const;

  /**
   * @brief Gets the memory usage of this source in bytes.
   *
   * @return Memory usage in bytes
   */
  size_t GetMemoryUsage() const;

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param image_data Variant containing the image data in row-major order
   * @param dimensions Image dimensions including width, height, and channels
   * @param tile_size Preferred tile size for efficient access
   */
  MemorySource(TileData image_data, ImageInfo dimensions, TileSize tile_size);
  TileData image_data_;   ///< Image data in row-major order (variant storage)
  ImageInfo dimensions_;  ///< Image dimensions
  TileSize tile_size_;    ///< Preferred tile size
};

// Template implementation
template <typename T>
MemorySource MemorySource::CreateTyped(std::vector<T> image_data, int width,
                                       int height, int channels,
                                       DataLayout layout, TileSize tile_size) {
  static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                    std::is_same_v<T, float>,
                "MemorySource only supports uint8_t, uint16_t, and float");

  PixelType pixel_type;
  if constexpr (std::is_same_v<T, uint8_t>) {
    pixel_type = PixelType::kUInt8;
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    pixel_type = PixelType::kUInt16;
  } else if constexpr (std::is_same_v<T, float>) {
    pixel_type = PixelType::kFloat32;
  }

  ImageInfo dims(width, height, channels, pixel_type, layout);
  TileData variant_data = std::move(image_data);
  return MemorySource(std::move(variant_data), dims, tile_size);
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SOURCES_MEMORY_SOURCE_H_

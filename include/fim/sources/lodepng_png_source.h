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
 * @file lodepng_png_source.h
 * @brief PNG image source implementation using lodepng for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the PngSource class which provides PNG image reading
 * capabilities for the fim image processing pipeline. It uses the lodepng
 * library for PNG decoding and supports lazy loading of image data.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SOURCES_LODEPNG_PNG_SOURCE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SOURCES_LODEPNG_PNG_SOURCE_H_

#include <vector>

#include "fim/pipeline.h"
#include "fim/types.h"

namespace fim {

/**
 * @brief PNG image source for reading PNG files using lodepng.
 *
 * This class provides a source implementation for PNG image files using
 * the lodepng library. It supports lazy loading of image data and provides
 * tile-based access to the image content. PNG files are loaded entirely
 * into memory only when first accessed (lazy materialization).
 *
 * The class follows the CRTP pattern by inheriting from SourceBase and
 * provides the required interface methods for image sources.
 *
 * @note Prefer using Create() factory method for error handling without
 * exceptions in future migrations.
 */
class PngSource : public SourceBase<PngSource> {
 public:
  /**
   * @brief Creates a PNG source from a file path (factory method).
   *
   * This is the only way to create a PngSource instance. The method validates
   * that the file exists before constructing the source.
   *
   * @param filename Path to the PNG file to read
   * @return PngSource instance
   * @throw std::runtime_error if the PNG file does not exist
   *
   * @note Future: This will be migrated to return absl::StatusOr<PngSource>
   * to avoid exceptions.
   */
  static PngSource Create(const fs::path& filename);

  /**
   * @brief Default destructor.
   */
  ~PngSource() = default;

  /**
   * @brief Deleted copy constructor.
   *
   * PNG sources cannot be copied to avoid unnecessary duplication of
   * image data and to ensure clear ownership semantics.
   */
  PngSource(const PngSource&) = delete;

  /**
   * @brief Deleted copy assignment operator.
   *
   * PNG sources cannot be copied to avoid unnecessary duplication of
   * image data and to ensure clear ownership semantics.
   */
  PngSource& operator=(const PngSource&) = delete;

  /**
   * @brief Move constructor.
   *
   * Transfers ownership of the PNG source and its loaded data to the
   * new instance. The source object is left in a valid but unspecified state.
   *
   * @param other PNG source to move from
   */
  PngSource(PngSource&& other) noexcept;

  /**
   * @brief Move assignment operator.
   *
   * Transfers ownership of the PNG source and its loaded data to this
   * instance. The source object is left in a valid but unspecified state.
   *
   * @param other PNG source to move from
   * @return Reference to this instance
   */
  PngSource& operator=(PngSource&& other) noexcept;

  /**
   * @brief Gets the dimensions of the PNG image.
   *
   * This method returns the complete dimensions of the PNG image including
   * width, height, and number of channels. PNG images are always loaded
   * with 4 channels (RGBA format). The image is loaded lazily on first call.
   *
   * @return ImageInfo containing the PNG image dimensions
   * @throw std::runtime_error if the PNG cannot be decoded
   */
  ImageInfo GetDimensions() const;

  /**
   * @brief Gets the ideal tile size for processing this PNG.
   *
   * PNG files are not inherently tiled, so this method returns the
   * full image dimensions as the ideal tile size to minimize memory
   * copies and maximize processing efficiency. The image is loaded lazily
   * on first call.
   *
   * @return TileSize containing the full image dimensions
   */
  TileSize GetIdealTileSize() const;

  /**
   * @brief Gets a tile from the PNG image.
   *
   * This method retrieves a rectangular region of the PNG image as a tile.
   * The tile coordinates are clamped to the image bounds. The PNG data
   * is loaded lazily into memory if not already loaded.
   *
   * @param x X coordinate of the top-left corner of the tile
   * @param y Y coordinate of the top-left corner of the tile
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile containing the requested PNG image data
   * @throw std::runtime_error if the PNG cannot be decoded
   */
  Tile GetTile(int x, int y, int width, int height) const;

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * The constructor validates file existence and initializes the PNG source
   * but doesn't load the image data. The actual loading is performed lazily
   * when image dimensions or tiles are first requested (lazy materialization).
   *
   * @param filename Path to the PNG file to read
   * @throw std::runtime_error if the PNG file does not exist
   */
  explicit PngSource(const fs::path& filename);
  /**
   * @brief Loads the PNG image data from file (lazy materialization).
   *
   * This method is called lazily when image data is first needed.
   * It uses lodepng to decode the PNG file and stores the result
   * in the internal data structures.
   *
   * @throw std::runtime_error if the PNG cannot be decoded
   */
  void LoadData() const;

  fs::path filename_;                        ///< Path to the PNG file
  mutable std::vector<uint8_t> image_data_;  ///< Decoded PNG image data
  mutable ImageInfo dimensions_;             ///< Cached image dimensions
  mutable bool data_loaded_;  ///< Whether image data has been loaded
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SOURCES_LODEPNG_PNG_SOURCE_H_

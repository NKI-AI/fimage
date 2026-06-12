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
 * @file crop.h
 * @brief Crop operator implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the Crop operator class which provides image cropping
 * capabilities for the fim image processing pipeline. It allows extracting
 * rectangular regions from input images with automatic bounds checking.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_CROP_H_
#define AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_CROP_H_

#include <algorithm>
#include <utility>

#include "fim/pipeline.h"
#include "fim/types.h"

namespace fim {

/**
 * @brief Crop operator for extracting rectangular regions from images.
 *
 * This operator extracts a rectangular region from the input image, specified
 * by the crop position and dimensions. The crop region is automatically
 * clamped to the input image bounds to prevent out-of-bounds access.
 *
 * The operator works by translating tile coordinates from the cropped output
 * space to the input image space and delegating tile requests to the input.
 * This allows for efficient processing without loading the entire image
 * into memory.
 *
 * @tparam InputType The type of the input source or operator
 */
template <typename InputType>
class Crop : public OperatorBase<Crop<InputType>, InputType> {
 public:
  /**
   * @brief Constructs a crop operator with the specified parameters.
   *
   * The crop region is defined by the top-left corner (x, y) and the
   * width and height of the region to extract. The crop region is
   * automatically clamped to the input image bounds. Takes ownership
   * of the input to ensure proper lifetime management.
   *
   * @param input The input source or operator to crop
   * @param x X coordinate of the top-left corner of the crop region
   * @param y Y coordinate of the top-left corner of the crop region
   * @param width Width of the crop region
   * @param height Height of the crop region
   */
  Crop(InputType input, int x, int y, int width, int height)
      : OperatorBase<Crop<InputType>, InputType>(std::move(input)),
        crop_x_(x),
        crop_y_(y),
        crop_width_(width),
        crop_height_(height) {}

  /**
   * @brief Gets the dimensions of the cropped output.
   *
   * This method returns the dimensions of the image after cropping.
   * The crop dimensions are automatically clamped to the input image
   * bounds to ensure valid output dimensions.
   *
   * @return ImageInfo containing the cropped image dimensions
   */
  ImageInfo GetDimensions() const {
    auto input_dims = this->input_.GetDimensions();

    // Clamp crop dimensions to input bounds
    int actual_width = std::min(crop_width_, input_dims.GetWidth() - crop_x_);
    int actual_height =
        std::min(crop_height_, input_dims.GetHeight() - crop_y_);

    actual_width = std::max(0, actual_width);
    actual_height = std::max(0, actual_height);

    return ImageInfo(actual_width, actual_height, input_dims.channels,
                     input_dims.pixel_type, input_dims.layout);
  }

  /**
   * @brief Gets the ideal tile size for processing the cropped output.
   *
   * This method returns the ideal tile size from the input source,
   * as the crop operation doesn't change the optimal processing
   * characteristics of the underlying data.
   *
   * @return TileSize containing the ideal tile dimensions
   */
  TileSize GetIdealTileSize() const { return this->input_.GetIdealTileSize(); }

  /**
   * @brief Gets a tile from the cropped output.
   *
   * This method retrieves a rectangular region of the cropped output
   * as a tile. The tile coordinates are translated from the output
   * space to the input space and the request is delegated to the input.
   *
   * @param x X coordinate of the top-left corner of the tile in output space
   * @param y Y coordinate of the top-left corner of the tile in output space
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile containing the requested cropped image data
   */
  Tile GetTile(int x, int y, int width, int height) const {
    // Translate tile coordinates to input image coordinates
    int input_x = crop_x_ + x;
    int input_y = crop_y_ + y;

    // Clamp the tile to the cropped region
    auto crop_dims = GetDimensions();
    int actual_width = std::min(width, crop_dims.GetWidth() - x);
    int actual_height = std::min(height, crop_dims.GetHeight() - y);

    if (actual_width <= 0 || actual_height <= 0) {
      return Tile(x, y, 0, 0, crop_dims.channels, crop_dims.layout,
                  crop_dims.pixel_type);
    }

    // Get tile from input and adjust coordinates for output space
    Tile tile =
        this->input_.GetTile(input_x, input_y, actual_width, actual_height);
    tile.x = x;
    tile.y = y;
    return tile;
  }

 private:
  int crop_x_;       ///< X coordinate of the top-left corner of the crop region
  int crop_y_;       ///< Y coordinate of the top-left corner of the crop region
  int crop_width_;   ///< Width of the crop region
  int crop_height_;  ///< Height of the crop region
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_CROP_H_

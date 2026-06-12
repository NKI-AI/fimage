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
 * @file stack.h
 * @brief Stack operator implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the Stack operator class which combines multiple images
 * by concatenating their channels (bands). This enables PyVips-style arbitrary
 * stacking of images in the processing pipeline.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_STACK_H_
#define AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_STACK_H_

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "fim/pipeline.h"
#include "fim/types.h"
#include "fim/utilities/layout_utils.h"

namespace fim {

/**
 * @brief Stack operator for combining multiple images along the channel axis.
 *
 * This operator takes multiple input sources/operators and combines them by
 * concatenating their channels. All inputs must have the same spatial
 * dimensions (width and height). The resulting image has the same dimensions
 * but with channels equal to the sum of all input channels.
 *
 * This enables PyVips-style operations like combining separate color channels
 * or stacking feature maps.
 *
 * @tparam InputType The type of the input sources or operators
 */
template <typename InputType>
class Stack : public OperatorBase<Stack<InputType>, InputType> {
 public:
  /**
   * @brief Constructs a stack operator with multiple inputs.
   *
   * All inputs must have the same spatial dimensions (width and height).
   * The operator will validate this during construction and throw an
   * exception if dimensions are incompatible. Takes ownership of all
   * inputs to ensure proper lifetime management.
   *
   * @param inputs Vector of input sources/operators to stack
   * @throw std::runtime_error if inputs have incompatible dimensions
   */
  explicit Stack(std::vector<InputType> inputs)
      : OperatorBase<Stack<InputType>, InputType>(inputs.empty() ? InputType()
                                                                 : inputs[0]),
        inputs_(std::move(inputs)) {
    if (inputs_.empty()) {
      throw std::runtime_error("Stack requires at least one input");
    }

    // Validate that all inputs have the same spatial dimensions
    auto first_dims = inputs_[0].GetDimensions();
    for (size_t i = 1; i < inputs_.size(); ++i) {
      auto dims = inputs_[i].GetDimensions();
      if (dims.GetWidth() != first_dims.GetWidth() ||
          dims.GetHeight() != first_dims.GetHeight()) {
        throw std::runtime_error(
            "All inputs to Stack must have the same width and height");
      }
      if (dims.layout != first_dims.layout) {
        throw std::runtime_error(
            "All inputs to Stack must have the same DataLayout. "
            "Use .ToLayout(...) / Image.to_layout(...) to convert explicitly.");
      }
      if (dims.pixel_type != first_dims.pixel_type) {
        throw std::runtime_error(
            "All inputs to Stack must have the same pixel type.");
      }
    }

    // Calculate total number of channels
    total_channels_ = 0;
    for (const auto& input : inputs_) {
      total_channels_ += input.GetDimensions().channels;
    }
  }

  /**
   * @brief Gets the dimensions of the stacked output.
   *
   * The output has the same width and height as the inputs, but with
   * channels equal to the sum of all input channels.
   *
   * @return ImageInfo containing the stacked image dimensions
   */
  ImageInfo GetDimensions() const {
    auto first_dims = inputs_[0].GetDimensions();
    return ImageInfo(first_dims.GetWidth(), first_dims.GetHeight(),
                     total_channels_, first_dims.pixel_type, first_dims.layout);
  }

  /**
   * @brief Gets the ideal tile size for processing the stacked output.
   *
   * Returns the ideal tile size from the first input, as the stack
   * operation doesn't change the optimal processing characteristics.
   *
   * @return TileSize containing the ideal tile dimensions
   */
  TileSize GetIdealTileSize() const { return inputs_[0].GetIdealTileSize(); }

  /**
   * @brief Gets a tile from the stacked output.
   *
   * This method retrieves tiles from all input sources at the same
   * spatial location and concatenates their channel data.
   *
   * @param x X coordinate of the top-left corner of the tile
   * @param y Y coordinate of the top-left corner of the tile
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile containing the stacked image data
   */
  Tile GetTile(int x, int y, int width, int height) const {
    // Get tiles from all inputs
    std::vector<Tile> input_tiles;
    input_tiles.reserve(inputs_.size());

    for (const auto& input : inputs_) {
      input_tiles.push_back(input.GetTile(x, y, width, height));
    }

    // All tiles should have the same spatial dimensions and pixel type
    // (validated by the inputs having the same dimensions)
    int tile_width = input_tiles[0].width;
    int tile_height = input_tiles[0].height;
    PixelType pixel_type = input_tiles[0].GetPixelType();
    DataLayout layout = input_tiles[0].layout;

    // Create output tile with concatenated channels
    Tile output_tile(x, y, tile_width, tile_height, total_channels_, layout,
                     pixel_type);

    // Concatenate channel data from all input tiles using visitor
    std::visit(
        [&](auto&& out_data) {
          using T = typename std::decay_t<decltype(out_data)>::value_type;
          if (layout == DataLayout::kChannelsLast) {
            size_t output_offset = 0;
            for (size_t pixel = 0; pixel < static_cast<size_t>(tile_width) *
                                               static_cast<size_t>(tile_height);
                 ++pixel) {
              for (const auto& input_tile : input_tiles) {
                const auto& in_data = input_tile.GetDataAs<T>();
                size_t input_offset =
                    pixel * static_cast<size_t>(input_tile.channels);
                for (int c = 0; c < input_tile.channels; ++c) {
                  out_data[output_offset++] = in_data[input_offset + c];
                }
              }
            }
            return;
          }

          // Channels-first: append planes sequentially.
          int out_c0 = 0;
          for (const auto& input_tile : input_tiles) {
            const auto& in_data = input_tile.GetDataAs<T>();
            for (int c = 0; c < input_tile.channels; ++c) {
              const int out_c = out_c0 + c;
              for (int row = 0; row < tile_height; ++row) {
                const size_t src_row_start =
                    (static_cast<size_t>(c) * static_cast<size_t>(tile_height) +
                     static_cast<size_t>(row)) *
                    static_cast<size_t>(tile_width);
                const size_t dst_row_start =
                    (static_cast<size_t>(out_c) *
                         static_cast<size_t>(tile_height) +
                     static_cast<size_t>(row)) *
                    static_cast<size_t>(tile_width);
                std::copy_n(&in_data[src_row_start],
                            static_cast<size_t>(tile_width),
                            &out_data[dst_row_start]);
              }
            }
            out_c0 += input_tile.channels;
          }
        },
        output_tile.GetVariantDataMut());

    return output_tile;
  }

 private:
  std::vector<InputType>
      inputs_;  ///< Vector of input sources/operators (owned by value)
  int total_channels_ = 0;  ///< Total number of output channels
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_STACK_H_

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
 * @file paste.h
 * @brief Paste operator implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the Paste operator class which composites a foreground
 * image onto a background image at a specified position. This enables
 * VIPs-style paste operations for building composite images from multiple
 * sources.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_PASTE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_PASTE_H_

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "fim/pipeline.h"
#include "fim/types.h"
#include "fim/utilities/layout_utils.h"

namespace fim {

/**
 * @brief Paste operator for compositing a foreground image onto a background.
 *
 * This operator takes two input sources/operators (background and foreground)
 * and composites the foreground onto the background at a specified position.
 * The foreground image completely replaces background pixels where it is
 * pasted.
 *
 * The operator validates that:
 * - Both images have the same number of channels
 * - The foreground region is fully contained within the background bounds
 *
 * This is a lazy operator - no pixels are actually composited until tiles are
 * requested by a sink.
 *
 * @tparam BackgroundType The type of the background source or operator
 * @tparam ForegroundType The type of the foreground source or operator
 */
template <typename BackgroundType, typename ForegroundType>
class Paste {
 public:
  /**
   * @brief Constructs a paste operator.
   *
   * Takes ownership of both the background and foreground inputs. Validates
   * that the foreground fits within the background bounds and that both have
   * the same number of channels.
   *
   * @param background The background source or operator
   * @param foreground The foreground source or operator to paste
   * @param paste_x X coordinate where foreground top-left will be placed
   * @param paste_y Y coordinate where foreground top-left will be placed
   * @throw std::invalid_argument if channels don't match or foreground is out
   * of bounds
   */
  Paste(BackgroundType background, ForegroundType foreground, int paste_x,
        int paste_y)
      : background_(std::move(background)),
        foreground_(std::move(foreground)),
        paste_x_(paste_x),
        paste_y_(paste_y) {
    // Get dimensions
    auto bg_dims = background_.GetDimensions();
    auto fg_dims = foreground_.GetDimensions();

    // Validate channel count matches
    if (bg_dims.channels != fg_dims.channels) {
      throw std::invalid_argument(
          "Paste: background and foreground must have the same number of "
          "channels (background: " +
          std::to_string(bg_dims.channels) +
          ", foreground: " + std::to_string(fg_dims.channels) + ")");
    }

    // Validate layout matches (otherwise we'd need an explicit conversion).
    if (bg_dims.layout != fg_dims.layout) {
      throw std::invalid_argument(
          "Paste: background and foreground must have the same DataLayout. "
          "Use .ToLayout(...) / Image.to_layout(...) to convert explicitly.");
    }

    // Validate foreground is fully within background bounds
    if (paste_x < 0 || paste_y < 0) {
      throw std::invalid_argument(
          "Paste: paste position must be non-negative (x: " +
          std::to_string(paste_x) + ", y: " + std::to_string(paste_y) + ")");
    }

    if (paste_x + fg_dims.GetWidth() > bg_dims.GetWidth() ||
        paste_y + fg_dims.GetHeight() > bg_dims.GetHeight()) {
      throw std::invalid_argument(
          "Paste: foreground image extends beyond background bounds "
          "(foreground: " +
          std::to_string(fg_dims.GetWidth()) + "x" +
          std::to_string(fg_dims.GetHeight()) + " at (" +
          std::to_string(paste_x) + ", " + std::to_string(paste_y) +
          "), background: " + std::to_string(bg_dims.GetWidth()) + "x" +
          std::to_string(bg_dims.GetHeight()) + ")");
    }

    // Store foreground region bounds for efficient overlap checking
    paste_x1_ = paste_x;
    paste_y1_ = paste_y;
    paste_x2_ = paste_x + fg_dims.GetWidth();
    paste_y2_ = paste_y + fg_dims.GetHeight();
  }

  /**
   * @brief Gets the dimensions of the output (same as background).
   *
   * @return ImageInfo containing the output dimensions
   */
  ImageInfo GetDimensions() const { return background_.GetDimensions(); }

  /**
   * @brief Gets the ideal tile size for processing.
   *
   * Returns the ideal tile size from the background, as the paste
   * operation doesn't change the optimal processing characteristics.
   *
   * @return TileSize containing the ideal tile dimensions
   */
  TileSize GetIdealTileSize() const { return background_.GetIdealTileSize(); }

  /**
   * @brief Renders the composited image to a sink.
   *
   * This method processes the entire paste pipeline and writes the result
   * to the specified sink. The sink determines the output format and
   * destination.
   *
   * @tparam SinkType The type of the sink (e.g., LodePngSink, TiffSink)
   * @param sink The sink to render to
   */
  template <typename SinkType>
  void Render(SinkType&& sink) {
    std::forward<SinkType>(sink).Render(*this);
  }

  /**
   * @brief Gets a tile from the composited output.
   *
   * This method checks if the requested tile overlaps with the pasted region.
   * If it does, it fetches tiles from both background and foreground and
   * composites them. Otherwise, it just returns the background tile.
   *
   * @param x X coordinate of the top-left corner of the tile
   * @param y Y coordinate of the top-left corner of the tile
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile containing the composited image data
   */
  Tile GetTile(int x, int y, int width, int height) const {
    // Check if this tile overlaps with the pasted region
    int tile_x2 = x + width;
    int tile_y2 = y + height;

    bool overlaps = !(tile_x2 <= paste_x1_ || x >= paste_x2_ ||
                      tile_y2 <= paste_y1_ || y >= paste_y2_);

    if (!overlaps) {
      // No overlap - just return background tile
      return background_.GetTile(x, y, width, height);
    }

    // There is overlap - need to composite
    // Get background tile
    Tile bg_tile = background_.GetTile(x, y, width, height);

    // Calculate the overlapping region in tile coordinates
    int overlap_x1 = std::max(x, paste_x1_);
    int overlap_y1 = std::max(y, paste_y1_);
    int overlap_x2 = std::min(tile_x2, paste_x2_);
    int overlap_y2 = std::min(tile_y2, paste_y2_);

    // Calculate corresponding region in foreground coordinates
    int fg_x = overlap_x1 - paste_x1_;
    int fg_y = overlap_y1 - paste_y1_;
    int fg_width = overlap_x2 - overlap_x1;
    int fg_height = overlap_y2 - overlap_y1;

    // Get foreground tile
    Tile fg_tile = foreground_.GetTile(fg_x, fg_y, fg_width, fg_height);

    // Composite: copy foreground pixels over background pixels
    // Use visitor to handle different pixel types
    std::visit(
        [&](auto&& bg_data) {
          using T = typename std::decay_t<decltype(bg_data)>::value_type;
          const auto& fg_data = fg_tile.GetDataAs<T>();
          int channels = bg_tile.channels;

          for (int ty = 0; ty < fg_tile.height; ++ty) {
            for (int tx = 0; tx < fg_tile.width; ++tx) {
              // Position in the output tile
              int out_x = (overlap_x1 - x) + tx;
              int out_y = (overlap_y1 - y) + ty;

              for (int c = 0; c < channels; ++c) {
                const size_t fg_idx =
                    layout_utils::Index(fg_tile.layout, fg_tile.width,
                                        fg_tile.height, channels, tx, ty, c);
                const size_t bg_idx = layout_utils::Index(
                    bg_tile.layout, bg_tile.width, bg_tile.height, channels,
                    out_x, out_y, c);
                bg_data[bg_idx] = fg_data[fg_idx];
              }
            }
          }
        },
        bg_tile.GetVariantDataMut());

    return bg_tile;
  }

 private:
  BackgroundType background_;  ///< Background source/operator (owned)
  ForegroundType foreground_;  ///< Foreground source/operator (owned)
  int paste_x_;                ///< X coordinate of paste position
  int paste_y_;                ///< Y coordinate of paste position
  int paste_x1_;               ///< Left edge of paste region
  int paste_y1_;               ///< Top edge of paste region
  int paste_x2_;               ///< Right edge of paste region (exclusive)
  int paste_y2_;               ///< Bottom edge of paste region (exclusive)
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_PASTE_H_

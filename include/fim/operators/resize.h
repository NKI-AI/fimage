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
 * @file resize.h
 * @brief Resize operator implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the Resize operator class which provides high-quality
 * image resizing capabilities using various resampling kernels (Lanczos,
 * Magic2021). It supports arbitrary output dimensions and subpixel-accurate
 * box parameters for precise region specification.
 *
 * @code
 * // Example usage:
 * auto source = fim::TiffSource::Create("input.tiff");
 *
 * // Basic resize to 512x512
 * auto resized = fim::Resize(source, 512, 512);
 *
 * // Resize with Magic2021 kernel
 * auto resized_magic = fim::Resize(source, 1024, 1024,
 *                                   fim::resize::KernelType::kMagic2021);
 *
 * // Resize with subpixel box for precise cropping and scaling
 * fim::resize::Box box{10.5F, 20.5F, 510.5F, 520.5F};
 * auto cropped_resized = fim::Resize(source, 256, 256,
 *                                     fim::resize::KernelType::kLanczos3,
 *                                     box);
 * @endcode
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_RESIZE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_RESIZE_H_

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "fim/operators/resize/kernels.h"
#include "fim/operators/resize/resample.h"
#include "fim/pipeline.h"
#include "fim/types.h"
#include "fim/utilities/buffer_pool.h"
#include "fim/utilities/layout_utils.h"

namespace fim {

namespace resize {

/**
 * @brief Cache key for resize plans.
 *
 * This key uniquely identifies a resize configuration to enable safe
 * plan reuse across tiles. It includes all parameters that affect the
 * tap weights: dimensions, kernel, scale factors, and subpixel phase.
 */
struct PlanKey {
  int in_w;           ///< Input width
  int in_h;           ///< Input height
  int out_w;          ///< Output width
  int out_h;          ///< Output height
  KernelType kernel;  ///< Resampling kernel type
  double scale_x;     ///< Horizontal scale factor
  double scale_y;     ///< Vertical scale factor
  double frac_x0;     ///< Fractional horizontal phase (quantized)
  double frac_y0;     ///< Fractional vertical phase (quantized)

  bool operator==(const PlanKey&) const = default;
};

/**
 * @brief Hash function for PlanKey.
 *
 * Combines all fields with quantization of floating-point values
 * to avoid issues with floating-point precision.
 */
struct PlanKeyHash {
  size_t operator()(const PlanKey& k) const {
    auto h = std::hash<int>{}(k.in_w) ^ (std::hash<int>{}(k.in_h) << 1);
    h ^= (std::hash<int>{}(k.out_w) << 2) ^ (std::hash<int>{}(k.out_h) << 3);
    h ^= (std::hash<int>{}(static_cast<int>(k.kernel)) << 4);

    // Coarse-quantize doubles to a grid to avoid FP noise
    auto quantize = [](double v) {
      return static_cast<int64_t>(std::llround(v * 1e6));
    };

    h ^= (std::hash<int64_t>{}(quantize(k.scale_x)) << 5);
    h ^= (std::hash<int64_t>{}(quantize(k.scale_y)) << 6);
    h ^= (std::hash<int64_t>{}(quantize(k.frac_x0)) << 7);
    h ^= (std::hash<int64_t>{}(quantize(k.frac_y0)) << 8);

    return h;
  }
};

}  // namespace resize

/**
 * @brief Resize operator for high-quality image resampling.
 *
 * This operator resizes images to specified output dimensions using
 * high-quality resampling kernels. It supports:
 * - Arbitrary output dimensions (upscaling and downscaling)
 * - Multiple kernel types (Lanczos2, Lanczos3, Magic2021)
 * - Subpixel-accurate box parameters for precise source region specification
 *
 * The operator works by:
 * 1. Mapping output tile coordinates to input coordinates
 * 2. Optionally applying box transformation for subpixel shifts
 * 3. Fetching the required input region
 * 4. Applying separable convolution resampling
 *
 * @tparam InputType The type of the input source or operator
 */
template <typename InputType>
class Resize : public OperatorBase<Resize<InputType>, InputType> {
 public:
  /**
   * @brief Constructs a resize operator.
   *
   * @param input The input source or operator to resize
   * @param target_width Target output width in pixels
   * @param target_height Target output height in pixels
   * @param kernel Resampling kernel to use (default: Lanczos3)
   * @param box Optional box parameter specifying source region in
   * floating-point pixel coordinates. If not provided, uses the entire input.
   * @throw std::invalid_argument if target dimensions are not positive, if
   * input dimensions are not positive, or if box has invalid dimensions
   */
  Resize(InputType input, int target_width, int target_height,
         resize::KernelType kernel = resize::KernelType::kLanczos3,
         std::optional<resize::Box> box = std::nullopt)
      : OperatorBase<Resize<InputType>, InputType>(std::move(input)),
        target_width_(target_width),
        target_height_(target_height),
        kernel_(kernel),
        box_(box) {
    if (target_width <= 0 || target_height <= 0) {
      throw std::invalid_argument("Resize target dimensions must be positive");
    }

    // Validate input dimensions
    auto input_dims = this->input_.GetDimensions();
    if (input_dims.GetWidth() <= 0 || input_dims.GetHeight() <= 0) {
      throw std::invalid_argument(
          "Resize input dimensions must be positive (got " +
          std::to_string(input_dims.GetWidth()) + "x" +
          std::to_string(input_dims.GetHeight()) + ")");
    }

    // Validate box if provided
    if (box_.has_value()) {
      const auto& b = box_.value();
      if (b.x2 <= b.x1 || b.y2 <= b.y1) {
        throw std::invalid_argument("Resize box must have positive dimensions");
      }
    }
  }

  /**
   * @brief Gets the dimensions of the resized output.
   *
   * Returns the target dimensions specified in the constructor.
   *
   * @return ImageInfo containing the target image dimensions
   */
  ImageInfo GetDimensions() const {
    auto input_dims = this->input_.GetDimensions();
    return ImageInfo(target_width_, target_height_, input_dims.channels,
                     input_dims.pixel_type, input_dims.layout);
  }

  /**
   * @brief Gets the ideal tile size for processing the resized output.
   *
   * The ideal tile size is computed based on the input's ideal tile size
   * and the scaling factor. For downscaling, we use smaller tiles; for
   * upscaling, we use larger tiles.
   *
   * @return TileSize containing the ideal tile dimensions for output
   */
  TileSize GetIdealTileSize() const {
    auto input_tile_size = this->input_.GetIdealTileSize();
    auto input_dims = this->input_.GetDimensions();

    // Compute scale factors
    const double scale_x =
        static_cast<double>(target_width_) / input_dims.GetWidth();
    const double scale_y =
        static_cast<double>(target_height_) / input_dims.GetHeight();

    // Scale the tile size accordingly, clamped to reasonable bounds
    int tile_width = static_cast<int>(input_tile_size.width * scale_x);
    int tile_height = static_cast<int>(input_tile_size.height * scale_y);

    tile_width = std::clamp(tile_width, 64, 2048);
    tile_height = std::clamp(tile_height, 64, 2048);

    return TileSize(tile_width, tile_height);
  }

  /**
   * @brief Gets a tile from the resized output.
   *
   * This method retrieves a rectangular region of the resized output as a tile.
   * It:
   * 1. Maps output coordinates to input coordinates (considering scale and box)
   * 2. Computes the required input region (with kernel support padding)
   * 3. Fetches the input tile
   * 4. Applies resampling to produce the output tile
   *
   * @param out_x X coordinate of the top-left corner in output space
   * @param out_y Y coordinate of the top-left corner in output space
   * @param out_width Width of the requested output tile
   * @param out_height Height of the requested output tile
   * @return Tile containing the resized image data
   */
  Tile GetTile(int out_x, int out_y, int out_width, int out_height) const {
    auto output_dims = GetDimensions();
    const DataLayout desired_layout = output_dims.layout;

    // Clamp tile bounds to output dimensions
    int actual_out_width = std::min(out_width, output_dims.GetWidth() - out_x);
    int actual_out_height =
        std::min(out_height, output_dims.GetHeight() - out_y);

    if (actual_out_width <= 0 || actual_out_height <= 0) {
      return Tile(out_x, out_y, 0, 0, output_dims.channels, output_dims.layout,
                  output_dims.pixel_type);
    }

    auto input_dims = this->input_.GetDimensions();

    // Determine the source region (box)
    resize::Box source_box;
    if (box_.has_value()) {
      source_box = box_.value();
    } else {
      source_box =
          resize::Box(0.0F, 0.0F, static_cast<float>(input_dims.GetWidth()),
                      static_cast<float>(input_dims.GetHeight()));
    }

    // Fast path for nearest neighbor interpolation
    if (kernel_ == resize::KernelType::kNearest) {
      const float box_width = source_box.x2 - source_box.x1;
      const float box_height = source_box.y2 - source_box.y1;
      const float scale_x = box_width / target_width_;
      const float scale_y = box_height / target_height_;

      // Compute input region with some padding for safety
      const int in_x = std::max(
          0, static_cast<int>(std::floor(source_box.x1 + out_x * scale_x)) - 1);
      const int in_y = std::max(
          0, static_cast<int>(std::floor(source_box.y1 + out_y * scale_y)) - 1);
      const int in_x2 = std::min(
          input_dims.GetWidth(),
          static_cast<int>(
              std::ceil(source_box.x1 + (out_x + actual_out_width) * scale_x)) +
              1);
      const int in_y2 =
          std::min(input_dims.GetHeight(),
                   static_cast<int>(std::ceil(
                       source_box.y1 + (out_y + actual_out_height) * scale_y)) +
                       1);
      const int in_width = in_x2 - in_x;
      const int in_height = in_y2 - in_y;

      // Fetch input tile
      Tile input_tile = this->input_.GetTile(in_x, in_y, in_width, in_height);
      if (desired_layout == DataLayout::kChannelsFirst) {
        input_tile = layout_utils::ConvertTileDataLayout(
            input_tile, DataLayout::kChannelsLast);
      }

      // Create output tile
      Tile output_tile(0, 0, actual_out_width, actual_out_height,
                       input_tile.channels, DataLayout::kChannelsLast,
                       input_tile.pixel_type);

      // Direct nearest-neighbor gather using pixel-center convention
      std::visit(
          [&](const auto& in_data_vec) {
            std::visit(
                [&](auto&& out_data_vec) {
                  for (int oy = 0; oy < actual_out_height; ++oy) {
                    // Map output y to source y using pixel-center convention
                    const double y_src =
                        source_box.y1 + ((out_y + oy + 0.5) * scale_y) - 0.5;
                    const int sy_global =
                        static_cast<int>(std::floor(y_src + 0.5));
                    const int sy_reflected =
                        resize::ReflectIndex(sy_global, input_dims.GetHeight());
                    const int sy_local = sy_reflected - in_y;
                    const int sy = std::clamp(sy_local, 0, in_height - 1);

                    for (int ox = 0; ox < actual_out_width; ++ox) {
                      // Map output x to source x using pixel-center convention
                      const double x_src =
                          source_box.x1 + ((out_x + ox + 0.5) * scale_x) - 0.5;
                      const int sx_global =
                          static_cast<int>(std::floor(x_src + 0.5));
                      const int sx_reflected = resize::ReflectIndex(
                          sx_global, input_dims.GetWidth());
                      const int sx_local = sx_reflected - in_x;
                      const int sx = std::clamp(sx_local, 0, in_width - 1);

                      // Copy all channels
                      for (int c = 0; c < input_tile.channels; ++c) {
                        const size_t in_idx =
                            (static_cast<size_t>(sy) * in_width + sx) *
                                input_tile.channels +
                            c;
                        const size_t out_idx =
                            (static_cast<size_t>(oy) * actual_out_width + ox) *
                                input_tile.channels +
                            c;
                        out_data_vec[out_idx] = in_data_vec[in_idx];
                      }
                    }
                  }
                },
                output_tile.GetVariantDataMut());
          },
          input_tile.GetVariantData());

      // Set correct tile position
      output_tile.x = out_x;
      output_tile.y = out_y;

      if (desired_layout == DataLayout::kChannelsFirst) {
        output_tile = layout_utils::ConvertTileDataLayout(
            output_tile, DataLayout::kChannelsFirst);
      }
      return output_tile;
    }

    // Compute input region needed for this output tile
    // Map output coordinates to box coordinates
    const float box_width = source_box.x2 - source_box.x1;
    const float box_height = source_box.y2 - source_box.y1;

    const float scale_x = box_width / target_width_;
    const float scale_y = box_height / target_height_;

    // Output tile region in box coordinates using pixel-center convention
    // x_src = x_box1 + (x_out + 0.5) * scale_x - 0.5
    const float tile_box_x1 = source_box.x1 + ((out_x + 0.5F) * scale_x) - 0.5F;
    const float tile_box_y1 = source_box.y1 + ((out_y + 0.5F) * scale_y) - 0.5F;
    const float tile_box_x2 =
        source_box.x1 + ((out_x + actual_out_width + 0.5F) * scale_x) - 0.5F;
    const float tile_box_y2 =
        source_box.y1 + ((out_y + actual_out_height + 0.5F) * scale_y) - 0.5F;

    // Add kernel support padding
    const double support = resize::GetKernelSupport(kernel_);
    const double filter_scale_x = std::max(1.0, static_cast<double>(scale_x));
    const double filter_scale_y = std::max(1.0, static_cast<double>(scale_y));
    const int padding_x =
        static_cast<int>(std::ceil(support * filter_scale_x)) + 1;
    const int padding_y =
        static_cast<int>(std::ceil(support * filter_scale_y)) + 1;

    // Compute input tile region with padding
    const int in_x =
        std::max(0, static_cast<int>(std::floor(tile_box_x1)) - padding_x);
    const int in_y =
        std::max(0, static_cast<int>(std::floor(tile_box_y1)) - padding_y);
    const int in_x2 =
        std::min(input_dims.GetWidth(),
                 static_cast<int>(std::ceil(tile_box_x2)) + padding_x);
    const int in_y2 =
        std::min(input_dims.GetHeight(),
                 static_cast<int>(std::ceil(tile_box_y2)) + padding_y);
    const int in_width = in_x2 - in_x;
    const int in_height = in_y2 - in_y;

    // Fetch input tile
    Tile input_tile = this->input_.GetTile(in_x, in_y, in_width, in_height);
    if (desired_layout == DataLayout::kChannelsFirst) {
      input_tile = layout_utils::ConvertTileDataLayout(
          input_tile, DataLayout::kChannelsLast);
    }

    // Create box for this tile (relative to the input tile)
    resize::Box tile_box(tile_box_x1 - in_x, tile_box_y1 - in_y,
                         tile_box_x2 - in_x, tile_box_y2 - in_y);

    // Compute cache key with fractional phase for correct tap reuse
    const double x_src0 = (tile_box_x1 + 0.5 * scale_x) - 0.5;
    const double y_src0 = (tile_box_y1 + 0.5 * scale_y) - 0.5;

    auto compute_frac = [](double v) {
      double f = std::fmod(v, 1.0);
      return (f < 0.0) ? f + 1.0 : f;
    };

    resize::PlanKey key{in_width,
                        in_height,
                        actual_out_width,
                        actual_out_height,
                        kernel_,
                        static_cast<double>(scale_x),
                        static_cast<double>(scale_y),
                        compute_frac(x_src0),
                        compute_frac(y_src0)};

    // Check cache and compute taps if needed (copy plan to avoid
    // use-after-free)
    resize::ResizePlan plan;
    {
      std::lock_guard<std::mutex> lock(*plan_mutex_);

      // Check if we have a cached plan with matching key
      auto it = plan_cache_.find(key);

      if (it == plan_cache_.end()) {
        // Compute taps for this tile configuration
        const std::vector<resize::Tap> taps_x = resize::ComputeTaps(
            actual_out_width, input_tile.width, kernel_, tile_box.x1,
            tile_box.x2, input_dims.GetWidth(), in_x);
        const std::vector<resize::Tap> taps_y = resize::ComputeTaps(
            actual_out_height, input_tile.height, kernel_, tile_box.y1,
            tile_box.y2, input_dims.GetHeight(), in_y);

        const int support_x =
            static_cast<int>(taps_x.size()) / std::max(1, actual_out_width);
        const int support_y =
            static_cast<int>(taps_y.size()) / std::max(1, actual_out_height);

        const size_t intermediate_size =
            static_cast<size_t>(actual_out_width) *
            static_cast<size_t>(input_tile.height) *
            static_cast<size_t>(input_tile.channels);

        resize::ResizePlan new_plan{taps_x,
                                    taps_y,
                                    support_x,
                                    support_y,
                                    static_cast<int>(intermediate_size),
                                    actual_out_width,
                                    actual_out_height,
                                    in_width,
                                    in_height};

        // Insert into cache and get iterator
        auto insert_result = plan_cache_.emplace(key, std::move(new_plan));
        it = insert_result.first;
      }

      // Copy the plan (not a pointer!) so it's safe to use outside the lock
      plan = it->second;
    }

    // Borrow intermediate buffer from pool
    auto intermediate = GetFloatBufferPool().GetBuffer(plan.intermediate_size);

    // Horizontal pass
    resize::HorizontalPass(input_tile, actual_out_width, plan.taps_x,
                           plan.support_x, intermediate.data());

    // Vertical pass
    Tile output_tile(0, 0, actual_out_width, actual_out_height,
                     input_tile.channels, DataLayout::kChannelsLast,
                     input_tile.pixel_type);
    resize::VerticalPass(intermediate.data(), actual_out_width,
                         input_tile.height, actual_out_height,
                         input_tile.channels, plan.taps_y, plan.support_y,
                         output_tile);

    // Return buffer to pool
    GetFloatBufferPool().ReturnBuffer(std::move(intermediate));

    // Set correct tile position
    output_tile.x = out_x;
    output_tile.y = out_y;

    if (desired_layout == DataLayout::kChannelsFirst) {
      output_tile = layout_utils::ConvertTileDataLayout(
          output_tile, DataLayout::kChannelsFirst);
    }
    return output_tile;
  }

 private:
  int target_width_;                ///< Target output width
  int target_height_;               ///< Target output height
  resize::KernelType kernel_;       ///< Resampling kernel to use
  std::optional<resize::Box> box_;  ///< Optional source box parameter

  // Cache for precomputed resize plans (mutable to allow caching in const
  // GetTile)
  mutable std::unordered_map<resize::PlanKey, resize::ResizePlan,
                             resize::PlanKeyHash>
      plan_cache_;
  mutable std::unique_ptr<std::mutex> plan_mutex_{
      std::make_unique<std::mutex>()};  ///< Protects plan_cache_
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_RESIZE_H_

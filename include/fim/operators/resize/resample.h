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
 * @file resample.h
 * @brief Separable convolution resampling for tiles.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file implements separable convolution resampling for fim::Tile objects,
 * supporting arbitrary scale factors and subpixel-accurate box parameters.
 * The implementation uses a two-pass approach (horizontal then vertical) for
 * efficiency.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_RESIZE_RESAMPLE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_RESIZE_RESAMPLE_H_

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "fim/operators/resize/kernels.h"
#include "fim/types.h"

namespace fim {
namespace resize {

/**
 * @brief Structure for precomputed resampling tap (index and weight).
 */
struct Tap {
  int idx;       ///< Input pixel index
  float weight;  ///< Kernel weight
};

/**
 * @brief Cached resize plan containing precomputed taps and metadata.
 *
 * This structure caches the precomputed horizontal and vertical taps
 * for a specific tile configuration, allowing reuse across multiple
 * GetTile calls with the same parameters.
 */
struct ResizePlan {
  std::vector<Tap> taps_x;  ///< Horizontal resampling taps
  std::vector<Tap> taps_y;  ///< Vertical resampling taps
  int support_x;            ///< Horizontal tap support
  int support_y;            ///< Vertical tap support
  int intermediate_size;    ///< Size of intermediate buffer needed
  int output_width;         ///< Output width for this plan
  int output_height;        ///< Output height for this plan
  int input_width;          ///< Input width for this plan
  int input_height;         ///< Input height for this plan
};

// Forward declarations for SIMD implementations
void HorizontalPassSIMD(const Tile& input, int output_width,
                        const std::vector<Tap>& taps_x, int support_x,
                        float* out_data);

void VerticalPassSIMD(const float* intermediate, int intermediate_width,
                      int intermediate_height, int output_height, int channels,
                      const std::vector<Tap>& taps_y, int support_y,
                      Tile& output);

/**
 * @brief Box parameter for subpixel-accurate source region specification.
 *
 * Specifies the source region in floating-point pixel coordinates.
 * For example, Box{0.5f, 0.5f, 100.5f, 100.5f} specifies a region
 * from pixel (0.5, 0.5) to (100.5, 100.5).
 */
struct Box {
  float x1 = 0.0F;  ///< Left edge in input pixel coordinates
  float y1 = 0.0F;  ///< Top edge in input pixel coordinates
  float x2 = 0.0F;  ///< Right edge in input pixel coordinates
  float y2 = 0.0F;  ///< Bottom edge in input pixel coordinates

  /**
   * @brief Default constructor creates an empty box.
   */
  Box() = default;

  /**
   * @brief Constructs a box with specified coordinates.
   *
   * @param x1_val Left edge
   * @param y1_val Top edge
   * @param x2_val Right edge
   * @param y2_val Bottom edge
   */
  Box(float x1_val, float y1_val, float x2_val, float y2_val)
      : x1(x1_val), y1(y1_val), x2(x2_val), y2(y2_val) {}

  /**
   * @brief Get width of the box.
   */
  float Width() const { return x2 - x1; }

  /**
   * @brief Get height of the box.
   */
  float Height() const { return y2 - y1; }
};

// NOTE: For palette/indexed images or label maps, NEAREST interpolation should
// be enforced to avoid interpolating discrete values. Consider adding a
// "discrete mode" parameter or auto-detecting based on pixel format in future.

/**
 * @brief Compute resampling taps for one dimension.
 *
 * This function precomputes the sample indices and weights for resampling
 * one dimension (either horizontal or vertical). It handles arbitrary
 * scale factors and optional box parameters for subpixel shifts.
 *
 * Border handling uses reflection against the original image boundaries,
 * not the tile boundaries, to ensure correct behavior at image edges.
 *
 * @param output_size Output dimension size
 * @param input_size Input dimension size (tile size)
 * @param kernel Kernel type to use
 * @param box_start Optional box start coordinate (for subpixel shift)
 * @param box_end Optional box end coordinate (for subpixel shift)
 * @param image_size Size of the full image (for proper boundary reflection)
 * @param tile_offset Offset of this tile in the full image
 * @return Vector of taps (length = output_size * support)
 */
inline std::vector<Tap> ComputeTaps(int output_size, int input_size,
                                    KernelType kernel, float box_start = 0.0F,
                                    float box_end = 0.0F, int image_size = -1,
                                    int tile_offset = 0) {
  // If box not specified, use full input range
  if (box_end <= box_start) {
    box_start = 0.0F;
    box_end = static_cast<float>(input_size);
  }

  // Use tile size as image size if not specified (backward compatibility)
  if (image_size < 0) {
    image_size = input_size;
  }

  const float box_width = box_end - box_start;
  const double scale = static_cast<double>(box_width) / output_size;
  const double filter_scale = std::max(1.0, scale);
  const double support = GetKernelSupport(kernel) * filter_scale;
  const int radius = static_cast<int>(std::ceil(support));
  const int tap_support = ((radius * 2) + 1);

  std::vector<Tap> taps(static_cast<size_t>(output_size) * tap_support);

  for (int out_idx = 0; out_idx < output_size; ++out_idx) {
    // Map output coordinate to box coordinate
    const double out_center = ((out_idx + 0.5) * scale) + box_start;
    const double in_center = out_center - 0.5;
    const int base = static_cast<int>(std::floor(in_center));

    // Compute weights for this output pixel
    float sum = 0.0f;
    Tap* pixel_taps = &taps[static_cast<size_t>(out_idx) * tap_support];

    for (int k = -radius; k <= radius; ++k) {
      const int tap_idx = k + radius;
      const int src_idx = base + k;

      // Clamp to image bounds
      const int global_idx = src_idx + tile_offset;
      const int clamped_global_idx = std::clamp(global_idx, 0, image_size - 1);

      // Convert back to tile-local coordinates
      const int tile_local_idx = clamped_global_idx - tile_offset;

      // Final clamp to tile bounds for safety
      pixel_taps[tap_idx].idx = std::clamp(tile_local_idx, 0, input_size - 1);

      // Compute distance from sample point to tap
      // where sample is at in_center, tap center is at (base + k + 0.5)
      const double frac = in_center - base;  // Fractional part of sample pos
      const double dist = (k - frac + 0.5) / filter_scale;
      const float weight =
          static_cast<float>(ComputeKernelWeight(kernel, dist));
      pixel_taps[tap_idx].weight = weight;
      sum += weight;
    }

    // Normalize weights to sum to 1
    if (sum > 1e-8F) {
      const float inv_sum = 1.0F / sum;
      for (int k = 0; k < tap_support; ++k) {
        pixel_taps[k].weight *= inv_sum;
      }
    } else {
      // Fallback: set center tap to 1
      const int center = tap_support / 2;
      for (int k = 0; k < tap_support; ++k) {
        pixel_taps[k].weight = (k == center) ? 1.0F : 0.0F;
      }
    }
  }

  return taps;
}

/**
 * @brief Perform horizontal resampling pass (scalar implementation).
 *
 * This is the scalar fallback implementation. The main HorizontalPass
 * function will use SIMD when available.
 *
 * @param input Input tile (must be kChannelsLast layout)
 * @param output_width Target output width
 * @param taps_x Precomputed horizontal taps
 * @param support_x Horizontal tap support
 * @param out_data Output buffer (must be pre-allocated to correct size)
 */
inline void HorizontalPassScalar(const Tile& input, int output_width,
                                 const std::vector<Tap>& taps_x, int support_x,
                                 float* out_data) {
  if (input.layout != DataLayout::kChannelsLast) {
    throw std::invalid_argument("HorizontalPass requires kChannelsLast layout");
  }

  const int in_height = input.height;
  const int in_width = input.width;
  const int channels = input.channels;

  // Use visitor to handle different input pixel types
  std::visit(
      [&](const auto& in_data_vec) {
        // Process each row
        for (int row = 0; row < in_height; ++row) {
          for (int out_x = 0; out_x < output_width; ++out_x) {
            const Tap* pixel_taps =
                &taps_x[static_cast<size_t>(out_x) * support_x];

            for (int chan = 0; chan < channels; ++chan) {
              double accum = 0.0;

              // Convolve with horizontal kernel
              for (int k = 0; k < support_x; ++k) {
                const int in_x = pixel_taps[k].idx;
                const size_t in_offset =
                    (static_cast<size_t>(row) * in_width + in_x) * channels +
                    chan;
                accum += static_cast<double>(in_data_vec[in_offset]) *
                         static_cast<double>(pixel_taps[k].weight);
              }

              const size_t out_offset =
                  (static_cast<size_t>(row) * output_width + out_x) * channels +
                  chan;
              out_data[out_offset] = static_cast<float>(accum);
            }
          }
        }
      },
      input.GetVariantData());
}

/**
 * @brief Perform horizontal resampling pass.
 *
 * Uses SIMD-optimized implementation via Highway for maximum performance.
 *
 * @param input Input tile (must be kChannelsLast layout)
 * @param output_width Target output width
 * @param taps_x Precomputed horizontal taps
 * @param support_x Horizontal tap support
 * @param out_data Output buffer (must be pre-allocated to correct size)
 */
inline void HorizontalPass(const Tile& input, int output_width,
                           const std::vector<Tap>& taps_x, int support_x,
                           float* out_data) {
  HorizontalPassSIMD(input, output_width, taps_x, support_x, out_data);
}

/**
 * @brief Perform vertical resampling pass (scalar implementation).
 *
 * This is the scalar fallback implementation. The main VerticalPass
 * function will use SIMD when available.
 *
 * @param intermediate Intermediate buffer from horizontal pass
 * @param intermediate_width Width of intermediate buffer (= output_width)
 * @param intermediate_height Height of intermediate buffer (= input_height)
 * @param output_height Target output height
 * @param channels Number of channels
 * @param taps_y Precomputed vertical taps
 * @param support_y Vertical tap support
 * @param output Output tile (must be pre-allocated with correct dimensions)
 */
inline void VerticalPassScalar(const float* intermediate,
                               int intermediate_width, int intermediate_height,
                               int output_height, int channels,
                               const std::vector<Tap>& taps_y, int support_y,
                               Tile& output) {
  const float* in_data = intermediate;

  // Dispatch based on pixel type for output
  std::visit(
      [&](auto&& out_data_vec) -> void {
        using T = typename std::decay_t<decltype(out_data_vec)>::value_type;

        // Process each column
        for (int out_y = 0; out_y < output_height; ++out_y) {
          const Tap* pixel_taps =
              &taps_y[static_cast<size_t>(out_y) * support_y];

          for (int col = 0; col < intermediate_width; ++col) {
            for (int chan = 0; chan < channels; ++chan) {
              double accum = 0.0;

              // Convolve with vertical kernel
              for (int k = 0; k < support_y; ++k) {
                const int in_y = pixel_taps[k].idx;
                const size_t in_offset =
                    (static_cast<size_t>(in_y) * intermediate_width + col) *
                        channels +
                    chan;
                accum += static_cast<double>(in_data[in_offset]) *
                         static_cast<double>(pixel_taps[k].weight);
              }

              const size_t out_offset =
                  (static_cast<size_t>(out_y) * intermediate_width + col) *
                      channels +
                  chan;
              out_data_vec[out_offset] = ToPixel<T>(accum);
            }
          }
        }
      },
      output.GetVariantDataMut());
}

/**
 * @brief Perform vertical resampling pass.
 *
 * Uses SIMD-optimized implementation via Highway for maximum performance.
 *
 * @param intermediate Intermediate buffer from horizontal pass
 * @param intermediate_width Width of intermediate buffer (= output_width)
 * @param intermediate_height Height of intermediate buffer (= input_height)
 * @param output_height Target output height
 * @param channels Number of channels
 * @param taps_y Precomputed vertical taps
 * @param support_y Vertical tap support
 * @param output Output tile (must be pre-allocated with correct dimensions)
 */
inline void VerticalPass(const float* intermediate, int intermediate_width,
                         int intermediate_height, int output_height,
                         int channels, const std::vector<Tap>& taps_y,
                         int support_y, Tile& output) {
  VerticalPassSIMD(intermediate, intermediate_width, intermediate_height,
                   output_height, channels, taps_y, support_y, output);
}

/**
 * @brief Resample a tile to a new size using separable convolution.
 *
 * This is the main entry point for resampling. It performs a two-pass
 * separable convolution (horizontal then vertical) to resize the input
 * tile to the specified output dimensions.
 *
 * NOTE: For RGBA images, consider implementing alpha premultiplication before
 * filtering and unpremultiplication after to avoid edge artifacts. This should
 * be skipped for NEAREST interpolation. A future enhancement could add an
 * optional parameter to enable/disable this behavior.
 *
 * @param input Input tile (must be kChannelsLast layout)
 * @param output_width Target output width
 * @param output_height Target output height
 * @param kernel Resampling kernel to use
 * @param box Optional box parameter for subpixel-accurate source region
 * @param image_width Width of the full image (for proper border reflection)
 * @param image_height Height of the full image (for proper border reflection)
 * @param tile_x X offset of this tile in the full image
 * @param tile_y Y offset of this tile in the full image
 * @return Resampled output tile
 * @throws std::invalid_argument if input layout is not kChannelsLast
 */
inline Tile ResampleTile(const Tile& input, int output_width, int output_height,
                         KernelType kernel, const Box* box = nullptr,
                         int image_width = -1, int image_height = -1,
                         int tile_x = 0, int tile_y = 0) {
  if (input.layout != DataLayout::kChannelsLast) {
    throw std::invalid_argument(
        "ResampleTile currently only supports kChannelsLast layout. "
        "kChannelsFirst support is not yet implemented.");
  }

  // Extract box parameters if provided
  float box_x1 = 0.0f;
  float box_y1 = 0.0f;
  float box_x2 = static_cast<float>(input.width);
  float box_y2 = static_cast<float>(input.height);

  if (box != nullptr) {
    box_x1 = box->x1;
    box_y1 = box->y1;
    box_x2 = box->x2;
    box_y2 = box->y2;
  }

  // Use tile size as image size if not specified (backward compatibility)
  if (image_width < 0) {
    image_width = input.width;
  }
  if (image_height < 0) {
    image_height = input.height;
  }

  // Compute resampling taps for both dimensions with global context
  const std::vector<Tap> taps_x = ComputeTaps(
      output_width, input.width, kernel, box_x1, box_x2, image_width, tile_x);
  const std::vector<Tap> taps_y =
      ComputeTaps(output_height, input.height, kernel, box_y1, box_y2,
                  image_height, tile_y);

  const int support_x =
      static_cast<int>(taps_x.size()) / std::max(1, output_width);
  const int support_y =
      static_cast<int>(taps_y.size()) / std::max(1, output_height);

  // Allocate intermediate buffer
  const size_t intermediate_size =
      static_cast<size_t>(output_width) * input.height * input.channels;
  std::vector<float> intermediate(intermediate_size);

  // Horizontal pass
  HorizontalPass(input, output_width, taps_x, support_x, intermediate.data());

  // Vertical pass
  Tile output(0, 0, output_width, output_height, input.channels,
              DataLayout::kChannelsLast, input.pixel_type);
  VerticalPass(intermediate.data(), output_width, input.height, output_height,
               input.channels, taps_y, support_y, output);

  return output;
}

}  // namespace resize
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_RESIZE_RESAMPLE_H_

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
 * @file memory_sink.h
 * @brief Memory sink implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the MemorySink class which provides in-memory rendering
 * capabilities for the fim image processing pipeline. It assembles the
 * pipeline output into a contiguous memory buffer using parallel tile
 * processing.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SINKS_MEMORY_SINK_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SINKS_MEMORY_SINK_H_

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <tuple>
#include <utility>
#include <vector>

#include "fim/pipeline.h"
#include "fim/types.h"
#include "fim/utilities/debug_flags.h"

namespace fim {

/**
 * @brief Memory sink for rendering images to a contiguous memory buffer.
 *
 * This class provides a sink implementation that assembles the entire pipeline
 * output into a contiguous memory buffer. It uses parallel tile processing
 * to efficiently fetch and assemble tiles, with each thread writing to
 * non-overlapping regions of the output buffer (no synchronization needed).
 *
 * The rendered buffer can be retrieved as a vector or moved out for zero-copy
 * transfer to Python/numpy or other consumers.
 *
 * The class follows the CRTP pattern by inheriting from SinkBase.
 *
 * @note Use Create() factory method to instantiate.
 */
class MemorySink : public SinkBase<MemorySink> {
 public:
  /**
   * @brief Creates a memory sink (factory method).
   *
   * This is the only way to create a MemorySink instance.
   *
   * @return MemorySink instance
   *
   * @note Future: This will be migrated to return absl::StatusOr<MemorySink>
   */
  static MemorySink Create();

  /**
   * @brief Default destructor.
   */
  ~MemorySink() = default;

  /**
   * @brief Renders the input to memory.
   *
   * This method processes the entire input pipeline and assembles it into
   * a contiguous memory buffer. The assembly is performed in parallel when
   * beneficial, with each thread writing to disjoint memory regions.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to render
   */
  template <typename InputType>
  void Render(const InputType& input);

  /**
   * @brief Gets typed access to the assembled image data.
   *
   * @tparam T The pixel type (uint8_t, uint16_t, or float)
   * @return Const reference to the typed image data vector
   * @throw std::bad_variant_access if the requested type doesn't match
   */
  template <typename T>
  const std::vector<T>& GetDataAs() const {
    return std::get<std::vector<T>>(assembled_data_);
  }

  /**
   * @brief Gets the dimensions of the rendered image.
   *
   * @return ImageInfo of the rendered output
   */
  const ImageInfo& GetDimensions() const { return dimensions_; }

  /**
   * @brief Moves the typed assembled data out of the sink.
   *
   * @tparam T The pixel type (uint8_t, uint16_t, or float)
   * @return The assembled image data (moved)
   * @throw std::bad_variant_access if the requested type doesn't match
   */
  template <typename T>
  std::vector<T> TakeDataAs() {
    return std::move(std::get<std::vector<T>>(assembled_data_));
  }

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   */
  MemorySink();

  /**
   * @brief Assembles the image data using parallel tile processing.
   *
   * This method coordinates parallel tile fetching and assembly. Each worker
   * thread processes a batch of tiles and writes them directly to
   * non-overlapping regions of the output buffer, avoiding the need for
   * synchronization.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to assemble
   */
  template <typename InputType>
  void AssembleImageData(const InputType& input);

  /**
   * @brief Assembles the image data serially (fallback for small images).
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to assemble
   */
  template <typename InputType>
  void AssembleImageDataSerial(const InputType& input);

  TileData assembled_data_;  ///< Assembled image buffer (variant storage)
  ImageInfo dimensions_;     ///< Dimensions of rendered image
};

template <typename InputType>
void MemorySink::Render(const InputType& input) {
  AssembleImageData(input);
}

template <typename InputType>
void MemorySink::AssembleImageData(const InputType& input) {
  dimensions_ = input.GetDimensions();
  auto dims = dimensions_;
  auto ideal_tile_size = input.GetIdealTileSize();

  // Use ideal tile size, but ensure we don't exceed image dimensions
  int tile_width = std::min(ideal_tile_size.width, dims.GetWidth());
  int tile_height = std::min(ideal_tile_size.height, dims.GetHeight());

  // If ideal tile size is 0, use reasonable defaults
  if (tile_width <= 0)
    tile_width = 256;
  if (tile_height <= 0)
    tile_height = 256;

  // Pre-allocate full output buffer with correct pixel type
  size_t num_elements = static_cast<size_t>(dims.GetWidth()) *
                        static_cast<size_t>(dims.GetHeight()) *
                        static_cast<size_t>(dims.channels);
  switch (dims.pixel_type) {
    case PixelType::kUInt8:
      assembled_data_ = std::vector<uint8_t>(num_elements);
      break;
    case PixelType::kUInt16:
      assembled_data_ = std::vector<uint16_t>(num_elements);
      break;
    case PixelType::kFloat32:
      assembled_data_ = std::vector<float>(num_elements);
      break;
    default:
      throw std::invalid_argument("Unsupported pixel type");
  }

  // Build list of tile coordinates
  std::vector<std::tuple<int, int, int, int>> tile_coords;
  for (int y = 0; y < dims.GetHeight(); y += tile_height) {
    for (int x = 0; x < dims.GetWidth(); x += tile_width) {
      int actual_width = std::min(tile_width, dims.GetWidth() - x);
      int actual_height = std::min(tile_height, dims.GetHeight() - y);
      tile_coords.push_back({x, y, actual_width, actual_height});
    }
  }

  const bool debug = debug_flags::IsEnabled("FIM_DEBUG_TO_NUMPY") ||
                     debug_flags::IsEnabled("FIM_DEBUG_MEMORY_SINK");
  const bool force_serial = debug_flags::IsEnabled("FIM_FORCE_SERIAL_SINK");
  const auto t0 = std::chrono::steady_clock::now();
  if (debug) {
    std::fprintf(stderr,
                 "[fim] MemorySink: dims=%dx%dx%d pixel_type=%d layout=%d "
                 "tile=%dx%d tiles=%zu force_serial=%d\n",
                 dims.GetWidth(), dims.GetHeight(), dims.channels,
                 static_cast<int>(dims.pixel_type),
                 static_cast<int>(dims.layout), tile_width, tile_height,
                 tile_coords.size(), force_serial ? 1 : 0);
    std::fflush(stderr);
  }

  // Check if we should parallelize
  if (!force_serial && ShouldParallelize(tile_coords.size())) {
    auto& pool = GetThreadPool();

    // Extract vector reference once before parallel processing to avoid
    // concurrent std::visit calls (which would be undefined behavior)
    std::visit(
        [&input, &tile_coords, &dims, &pool](auto& assembled_vec) {
          using T = typename std::decay_t<decltype(assembled_vec)>::value_type;

          // Simple parallel processing: distribute tiles directly to workers
          // Benchmarks show this is ~13% faster than block-based approaches
          // The thread pool's submit_blocks already provides good work
          // distribution
          auto futures = pool.submit_blocks(
              0, tile_coords.size(),
              [&input, &tile_coords, &dims, &assembled_vec](size_t start,
                                                            size_t end) {
                for (size_t i = start; i < end; ++i) {
                  const auto& coord = tile_coords[i];
                  int x = std::get<0>(coord);
                  int y = std::get<1>(coord);
                  int width = std::get<2>(coord);
                  int height = std::get<3>(coord);

                  // Fetch tile from pipeline
                  Tile tile = input.GetTile(x, y, width, height);
                  const auto& tile_data = tile.GetDataAs<T>();

                  if (dims.layout == DataLayout::kChannelsLast) {
                    // Write directly to output buffer at non-overlapping region
                    // (HWC). No synchronization needed: each thread writes to
                    // disjoint memory.
                    for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
                      size_t src_row_start = static_cast<size_t>(tile_y) *
                                             static_cast<size_t>(tile.width) *
                                             static_cast<size_t>(dims.channels);
                      size_t dst_row_start =
                          (static_cast<size_t>(y + tile_y) *
                               static_cast<size_t>(dims.GetWidth()) +
                           static_cast<size_t>(x)) *
                          static_cast<size_t>(dims.channels);
                      std::copy_n(&tile_data[src_row_start],
                                  static_cast<size_t>(tile.width) *
                                      static_cast<size_t>(dims.channels),
                                  &assembled_vec[dst_row_start]);
                    }
                  } else {
                    // Channels-first (CHW)
                    for (int c = 0; c < dims.channels; ++c) {
                      for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
                        size_t src_row_start =
                            (static_cast<size_t>(c) *
                                 static_cast<size_t>(tile.height) +
                             static_cast<size_t>(tile_y)) *
                            static_cast<size_t>(tile.width);
                        size_t dst_row_start =
                            (static_cast<size_t>(c) *
                                 static_cast<size_t>(dims.GetHeight()) +
                             static_cast<size_t>(y + tile_y)) *
                                static_cast<size_t>(dims.GetWidth()) +
                            static_cast<size_t>(x);
                        std::copy_n(&tile_data[src_row_start],
                                    static_cast<size_t>(tile.width),
                                    &assembled_vec[dst_row_start]);
                      }
                    }
                  }
                }
              });

          // Wait for all workers to complete and propagate exceptions
          for (auto& future : futures) {
            future.get();  // Use get() instead of wait() to surface exceptions
          }
        },
        assembled_data_);
  } else {
    // Serial path for small images
    AssembleImageDataSerial(input);
  }

  if (debug) {
    const auto t1 = std::chrono::steady_clock::now();
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::fprintf(stderr, "[fim] MemorySink: done in %lld ms\n",
                 static_cast<long long>(ms));
    std::fflush(stderr);
  }
}

template <typename InputType>
void MemorySink::AssembleImageDataSerial(const InputType& input) {
  auto dims = input.GetDimensions();
  auto ideal_tile_size = input.GetIdealTileSize();

  int tile_width = std::min(ideal_tile_size.width, dims.GetWidth());
  int tile_height = std::min(ideal_tile_size.height, dims.GetHeight());

  if (tile_width <= 0)
    tile_width = 256;
  if (tile_height <= 0)
    tile_height = 256;

  // Process image in tiles serially
  for (int y = 0; y < dims.GetHeight(); y += tile_height) {
    for (int x = 0; x < dims.GetWidth(); x += tile_width) {
      int actual_width = std::min(tile_width, dims.GetWidth() - x);
      int actual_height = std::min(tile_height, dims.GetHeight() - y);

      // Get tile from input
      Tile tile = input.GetTile(x, y, actual_width, actual_height);

      // Copy tile data to appropriate position in full image
      // Using visitor pattern to handle different pixel types
      std::visit(
          [&tile, &dims, x, y](auto& assembled_vec) {
            using T =
                typename std::decay_t<decltype(assembled_vec)>::value_type;
            const auto& tile_data = tile.GetDataAs<T>();

            if (dims.layout == DataLayout::kChannelsLast) {
              // Using row-wise copy for better performance (HWC)
              for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
                size_t src_row_start = static_cast<size_t>(tile_y) *
                                       static_cast<size_t>(tile.width) *
                                       static_cast<size_t>(dims.channels);
                size_t dst_row_start =
                    (static_cast<size_t>(y + tile_y) *
                         static_cast<size_t>(dims.GetWidth()) +
                     static_cast<size_t>(x)) *
                    static_cast<size_t>(dims.channels);
                std::copy_n(&tile_data[src_row_start],
                            static_cast<size_t>(tile.width) *
                                static_cast<size_t>(dims.channels),
                            &assembled_vec[dst_row_start]);
              }
            } else {
              // Channels-first (CHW)
              for (int c = 0; c < dims.channels; ++c) {
                for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
                  size_t src_row_start = (static_cast<size_t>(c) *
                                              static_cast<size_t>(tile.height) +
                                          static_cast<size_t>(tile_y)) *
                                         static_cast<size_t>(tile.width);
                  size_t dst_row_start =
                      (static_cast<size_t>(c) *
                           static_cast<size_t>(dims.GetHeight()) +
                       static_cast<size_t>(y + tile_y)) *
                          static_cast<size_t>(dims.GetWidth()) +
                      static_cast<size_t>(x);
                  std::copy_n(&tile_data[src_row_start],
                              static_cast<size_t>(tile.width),
                              &assembled_vec[dst_row_start]);
                }
              }
            }
          },
          assembled_data_);
    }
  }
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SINKS_MEMORY_SINK_H_

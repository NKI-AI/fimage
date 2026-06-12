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
 * @file spng_sink.h
 * @brief Streaming PNG image sink implementation using libspng for the fim
 * library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the SpngSink class which provides streaming PNG image
 * writing capabilities for the fim image processing pipeline. It uses the
 * libspng library for PNG encoding and supports grayscale, RGB, and RGBA
 * formats.
 *
 * Unlike LodePngSink, this implementation uses scanline-based streaming
 * encoding, which means it processes the image row-by-row and never keeps
 * the full uncompressed image in memory. This is crucial for very large
 * whole slide images where memory usage must be minimized.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SINKS_SPNG_SINK_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SINKS_SPNG_SINK_H_

#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "aifocore/platform/portability.h"
#include "fim/pipeline.h"
#include "fim/pixel_type_traits.h"
#include "fim/types.h"
#include "spng.h"

namespace fim {

/**
 * @brief Streaming PNG image sink for writing PNG files using libspng.
 *
 * This class provides a sink implementation for PNG image files using
 * the libspng library with scanline-based streaming encoding. It supports
 * grayscale (1 channel), RGB (3 channels), and RGBA (4 channels) formats.
 * The sink processes the image row-by-row, only keeping one scanline in
 * memory at a time.
 *
 * The class follows the CRTP pattern by inheriting from SinkBase and
 * provides the required interface methods for image sinks.
 *
 * Memory Considerations:
 * Unlike LodePngSink which requires the full uncompressed image in memory,
 * SpngSink uses streaming/scanline-based encoding. This means only the
 * current row is kept in memory, making it suitable for gigapixel whole
 * slide images where memory efficiency is critical.
 *
 * @note Use Create() factory method to instantiate.
 */
class SpngSink : public SinkBase<SpngSink> {
 public:
  /**
   * @brief Creates a PNG sink with the specified output filename (factory
   * method).
   *
   * This is the only way to create a SpngSink instance.
   *
   * @param filename Path to the output PNG file where the image will be
   * written
   * @return SpngSink instance
   *
   * @note Future: This will be migrated to return absl::StatusOr<SpngSink>
   * to avoid exceptions.
   */
  static SpngSink Create(const fs::path& filename);

  /**
   * @brief Destructor - ensures proper cleanup of libspng resources.
   */
  ~SpngSink();

  /**
   * @brief Deleted copy constructor - SpngSink is not copyable due to file
   * handle.
   */
  SpngSink(const SpngSink&) = delete;

  /**
   * @brief Deleted copy assignment - SpngSink is not copyable due to file
   * handle.
   */
  SpngSink& operator=(const SpngSink&) = delete;

  /**
   * @brief Move constructor.
   */
  SpngSink(SpngSink&& other) noexcept;

  /**
   * @brief Move assignment operator.
   */
  SpngSink& operator=(SpngSink&& other) noexcept;

  /**
   * @brief Renders the input to a PNG file using streaming encoding.
   *
   * This method processes the input row-by-row, fetching tiles as needed
   * and assembling them into scanlines, then writes each scanline to the
   * PNG file using libspng. The method supports 1, 3, and 4 channel images.
   *
   * Memory usage is minimal - only one scanline is kept in memory at a time,
   * making this suitable for very large images.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to render
   * @throw std::runtime_error if the image dimensions are invalid or PNG
   * writing fails
   */
  template <typename InputType>
  void Render(const InputType& input);

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param filename Path to the output PNG file where the image will be
   * written
   */
  explicit SpngSink(const fs::path& filename);

  /**
   * @brief Encodes the image row-by-row using libspng streaming API.
   *
   * This method processes the image scanline by scanline, fetching tiles
   * as needed to assemble each row, then immediately writing it to the
   * PNG file using spng_encode_row(). This ensures minimal memory usage.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to encode
   * @param ctx The libspng encoder context
   */
  template <typename InputType>
  void EncodeRowByRow(const InputType& input, spng_ctx* ctx);

  /**
   * @brief Assembles a single row from tiles.
   *
   * This method fetches the necessary tiles to assemble one complete
   * scanline of the image.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator
   * @param row_y The row number to assemble
   * @param tile_width The width of tiles to fetch
   * @param row_buffer Output buffer for the assembled row
   */
  template <typename InputType>
  void AssembleRow(const InputType& input, int row_y, int tile_width,
                   std::vector<uint8_t>& row_buffer);

  /**
   * @brief Maps fim channel count to spng color type.
   *
   * @param channels Number of channels (1, 3, or 4)
   * @return Corresponding spng_color_type
   * @throw std::runtime_error if channel count is unsupported
   */
  static spng_color_type MapChannelsToColorType(int channels);
};

template <typename InputType>
void SpngSink::Render(const InputType& input) {
  auto dims = input.GetDimensions();

  if (dims.GetWidth() <= 0 || dims.GetHeight() <= 0) {
    throw std::runtime_error("Invalid image dimensions for PNG output");
  }

  if (dims.layout != DataLayout::kChannelsLast) {
    throw std::runtime_error(
        "PNG output currently only supports DataLayout::kChannelsLast "
        "(interleaved/HWC). Use .ToLayout(kChannelsLast) / Image.to_layout().");
  }

  // Create and configure spng encoder context
  spng_ctx* ctx = spng_ctx_new(SPNG_CTX_ENCODER);
  if (!ctx) {
    throw std::runtime_error("Failed to create spng encoder context");
  }

  // RAII cleanup wrapper
  auto ctx_deleter = [](spng_ctx* c) {
    spng_ctx_free(c);
  };
  std::unique_ptr<spng_ctx, decltype(ctx_deleter)> ctx_guard(ctx, ctx_deleter);

  // Open output file
  FILE* file_ptr = aifocore::portable_fopen(filename_, "wb");  // NOLINT
  if (!file_ptr) {
    throw std::runtime_error("Failed to open output file: " +
                             filename_.string());
  }

  // RAII cleanup wrapper for file
  auto file_deleter = [](FILE* file) {
    aifocore::portable_fclose(file);
  };  // NOLINT
  std::unique_ptr<FILE, decltype(file_deleter)> file_guard(file_ptr,
                                                           file_deleter);

  // Set output file
  int ret = spng_set_png_file(ctx, file_ptr);
  if (ret != 0) {
    throw std::runtime_error("Failed to set PNG file: " +
                             std::string(spng_strerror(ret)));
  }

  // Validate pixel type for PNG
  if (dims.pixel_type != PixelType::kUInt8 &&
      dims.pixel_type != PixelType::kUInt16) {
    throw std::runtime_error(
        "PNG output only supports uint8 and uint16 pixel types. "
        "Got: " +
        GetPixelTypeName(dims.pixel_type) +
        ". For float32 images, convert to uint8/uint16 or use a different "
        "format (e.g., TIFF or FImage).");
  }

  // Configure image header
  spng_ihdr ihdr = {};
  ihdr.width = dims.GetWidth();
  ihdr.height = dims.GetHeight();
  ihdr.bit_depth = (dims.pixel_type == PixelType::kUInt16) ? 16 : 8;
  ihdr.color_type = MapChannelsToColorType(dims.channels);

  ret = spng_set_ihdr(ctx, &ihdr);
  if (ret != 0) {
    throw std::runtime_error("Failed to set IHDR: " +
                             std::string(spng_strerror(ret)));
  }

  // Initialize progressive encoding mode
  // SPNG_ENCODE_FINALIZE ensures the PNG is finalized (IEND marker written)
  // after all rows are encoded via spng_encode_row()
  ret = spng_encode_image(ctx, nullptr, 0, SPNG_FMT_RAW,
                          SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE);
  if (ret != 0) {
    throw std::runtime_error("Failed to initialize progressive encoding: " +
                             std::string(spng_strerror(ret)));
  }

  // Encode image row-by-row (streaming)
  // spng_encode_row() returns SPNG_EOI when the last row is encoded
  EncodeRowByRow(input, ctx);

  // Flush the file to ensure all data is written
  std::fflush(file_ptr);
}

template <typename InputType>
void SpngSink::EncodeRowByRow(const InputType& input, spng_ctx* ctx) {
  auto dims = input.GetDimensions();
  auto ideal_tile_size = input.GetIdealTileSize();

  // Use ideal tile size, but ensure we don't exceed image dimensions
  int tile_width = std::min(ideal_tile_size.width, dims.GetWidth());

  // If ideal tile size is 0, use reasonable default
  if (tile_width <= 0) {
    tile_width = 256;
  }

  // Calculate row width in bytes (accounting for pixel size)
  size_t pixel_size = GetPixelTypeSize(dims.pixel_type);
  size_t row_width_bytes = static_cast<size_t>(dims.GetWidth()) *
                           static_cast<size_t>(dims.channels) * pixel_size;

  // Allocate buffer for a single row (in bytes)
  std::vector<uint8_t> row_buffer(row_width_bytes);

  // Process image row by row
  for (int row_y = 0; row_y < dims.GetHeight(); ++row_y) {
    // Assemble the current row from tiles
    AssembleRow(input, row_y, tile_width, row_buffer);

    // Write this row using libspng streaming API
    // spng_encode_row returns SPNG_EOI for the last row, which is success
    int ret = spng_encode_row(ctx, row_buffer.data(), row_width_bytes);
    if (ret != 0 && ret != SPNG_EOI) {
      throw std::runtime_error("Failed to encode row " + std::to_string(row_y) +
                               ": " + std::string(spng_strerror(ret)));
    }
  }
}

template <typename InputType>
void SpngSink::AssembleRow(const InputType& input, int row_y, int tile_width,
                           std::vector<uint8_t>& row_buffer) {
  auto dims = input.GetDimensions();

  // Process this row tile by tile
  for (int tile_x = 0; tile_x < dims.GetWidth(); tile_x += tile_width) {
    int actual_width = std::min(tile_width, dims.GetWidth() - tile_x);

    // Get a tile that covers this portion of the row
    // We request height=1 for efficiency, but some sources may return larger
    // tiles
    Tile tile = input.GetTile(tile_x, row_y, actual_width, 1);

    // Copy the first row of the tile to our row buffer
    // (Some sources might return multi-row tiles even when we request height=1)
    int copy_width = std::min(tile.width, actual_width);

    // Use visitor to handle different pixel types
    std::visit(
        [&](const auto& tile_data) {
          using T = typename std::decay_t<decltype(tile_data)>::value_type;
          size_t pixel_size = sizeof(T);

          for (int pixel_x = 0; pixel_x < copy_width; ++pixel_x) {
            size_t src_offset = pixel_x * dims.channels;
            size_t dst_offset = (tile_x + pixel_x) * dims.channels;

            // Copy pixel data as bytes (works for both uint8 and uint16)
            const uint8_t* src_bytes =
                reinterpret_cast<const uint8_t*>(&tile_data[src_offset]);
            uint8_t* dst_bytes = &row_buffer[dst_offset * pixel_size];

            std::memcpy(dst_bytes, src_bytes, dims.channels * pixel_size);
          }
        },
        tile.GetVariantData());
  }
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SINKS_SPNG_SINK_H_

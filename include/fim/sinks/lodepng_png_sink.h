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
 * @file lodepng_png_sink.h
 * @brief PNG image sink implementation using lodepng for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the LodePngSink class which provides PNG image writing
 * capabilities for the fim image processing pipeline. It uses the lodepng
 * library for PNG encoding and supports grayscale, RGB, and RGBA formats.
 *
 * Note: lodepng requires the full uncompressed image buffer in memory before
 * encoding. For true streaming (scanline-based) PNG encoding with lower memory
 * usage, consider using libpng instead.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SINKS_LODEPNG_PNG_SINK_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SINKS_LODEPNG_PNG_SINK_H_

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "aifocore/platform/portability.h"
#include "fim/pipeline.h"
#include "fim/pixel_type_traits.h"
#include "fim/types.h"
#include "lodepng/lodepng.h"

namespace fim {

/**
 * @brief PNG image sink for writing PNG files using lodepng.
 *
 * This class provides a sink implementation for PNG image files using
 * the lodepng library. It supports grayscale (1 channel), RGB (3 channels),
 * and RGBA (4 channels) formats. The sink assembles the complete image
 * from tiles and writes it to a PNG file.
 *
 * The class follows the CRTP pattern by inheriting from SinkBase and
 * provides the required interface methods for image sinks.
 *
 * Memory Considerations:
 * lodepng requires the full uncompressed image in memory before encoding,
 * which means very large images (e.g., gigapixel whole slide images) will
 * consume significant memory. For streaming/scanline-based encoding, use
 * TiffSink instead or consider migrating to libpng in the future.
 *
 * @note Use Create() factory method to instantiate.
 */
class LodePngSink : public SinkBase<LodePngSink> {
 public:
  /**
   * @brief Creates a PNG sink with the specified output filename (factory
   * method).
   *
   * This is the only way to create a LodePngSink instance.
   *
   * @param filename Path to the output PNG file where the image will be written
   * @return LodePngSink instance
   *
   * @note Future: This will be migrated to return absl::StatusOr<LodePngSink>
   * to avoid exceptions.
   */
  static LodePngSink Create(const fs::path& filename);

  /**
   * @brief Default destructor.
   */
  ~LodePngSink() = default;

  /**
   * @brief Renders the input to a PNG file.
   *
   * This method processes the entire input by fetching tiles and assembling
   * them into a complete image, then writes the result to a PNG file using
   * lodepng. The method supports 1, 3, and 4 channel images.
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
   * @brief Assembles the complete image from tiles.
   *
   * This method fetches tiles from the input source and assembles them
   * into a complete image buffer. It uses the input's ideal tile size
   * to optimize the tiling process.
   *
   * Note: This pre-allocates the full image buffer as required by lodepng.
   * For large images, this can consume significant memory.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to assemble
   * @param image_data Output vector to store the assembled image data
   */
  template <typename InputType>
  void AssembleImage(const InputType& input, std::vector<uint8_t>& image_data);

  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param filename Path to the output PNG file where the image will be written
   */
  explicit LodePngSink(const fs::path& filename);
};

template <typename InputType>
void LodePngSink::Render(const InputType& input) {
  auto dims = input.GetDimensions();

  if (dims.GetWidth() <= 0 || dims.GetHeight() <= 0) {
    throw std::runtime_error("Invalid image dimensions for PNG output");
  }

  if (dims.layout != DataLayout::kChannelsLast) {
    throw std::runtime_error(
        "PNG output currently only supports DataLayout::kChannelsLast "
        "(interleaved/HWC). Use .ToLayout(kChannelsLast) / Image.to_layout().");
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

  // Assemble full image data
  std::vector<uint8_t> image_data;
  AssembleImage(input, image_data);

  // Configure encoder to preserve color type (disable auto_convert)
  LodePNGState state;
  lodepng_state_init(&state);
  state.encoder.auto_convert = 0;  // Disable automatic color type conversion

  // Determine bit depth from pixel type
  unsigned bit_depth = (dims.pixel_type == PixelType::kUInt16) ? 16 : 8;

  // Set the target color type based on channels
  if (dims.channels == 1) {
    state.info_png.color.colortype = LCT_GREY;
    state.info_png.color.bitdepth = bit_depth;
  } else if (dims.channels == 3) {
    state.info_png.color.colortype = LCT_RGB;
    state.info_png.color.bitdepth = bit_depth;
  } else if (dims.channels == 4) {
    state.info_png.color.colortype = LCT_RGBA;
    state.info_png.color.bitdepth = bit_depth;
  } else {
    lodepng_state_cleanup(&state);
    throw std::runtime_error("Unsupported channel count for PNG output: " +
                             std::to_string(dims.channels));
  }

  // Set raw color type to match (this is the format of our input data)
  state.info_raw.colortype = state.info_png.color.colortype;
  state.info_raw.bitdepth = bit_depth;

  // Encode to memory buffer first
  unsigned char* png_buffer = nullptr;
  size_t png_size = 0;
  unsigned png_error =
      lodepng_encode(&png_buffer, &png_size, image_data.data(), dims.GetWidth(),
                     dims.GetHeight(), &state);

  if (png_error != 0) {
    if (png_buffer)
      free(png_buffer);
    lodepng_state_cleanup(&state);
    throw std::runtime_error("Failed to encode PNG: " +
                             std::string(lodepng_error_text(png_error)));
  }

  // Write buffer to file (portable across Windows/POSIX).
  FILE* file_ptr = aifocore::portable_fopen(filename_, "wb");  // NOLINT
  if (!file_ptr) {
    free(png_buffer);
    lodepng_state_cleanup(&state);
    throw std::runtime_error("Failed to open output file: " +
                             filename_.string());
  }
  const size_t bytes_written =
      aifocore::portable_fwrite(png_buffer, png_size, file_ptr);
  aifocore::portable_fclose(file_ptr);
  png_error = (bytes_written == png_size) ? 0 : 1;

  free(png_buffer);
  lodepng_state_cleanup(&state);

  if (png_error != 0) {
    throw std::runtime_error("Failed to write PNG file " + filename_.string() +
                             " (error " + std::to_string(png_error) +
                             "): " + lodepng_error_text(png_error));
  }
}

template <typename InputType>
void LodePngSink::AssembleImage(const InputType& input,
                                std::vector<uint8_t>& image_data) {
  auto dims = input.GetDimensions();
  auto ideal_tile_size = input.GetIdealTileSize();

  // Use ideal tile size, but ensure we don't exceed image dimensions
  int tile_width = std::min(ideal_tile_size.width, dims.GetWidth());
  int tile_height = std::min(ideal_tile_size.height, dims.GetHeight());

  // If ideal tile size is 0, use reasonable defaults
  if (tile_width <= 0)
    tile_width = 256;
  if (tile_height <= 0)
    tile_height = 256;

  // Pre-allocate full image buffer (required by lodepng, accounting for pixel
  // size)
  size_t pixel_size = GetPixelTypeSize(dims.pixel_type);
  size_t num_bytes = static_cast<size_t>(dims.GetWidth()) *
                     static_cast<size_t>(dims.GetHeight()) *
                     static_cast<size_t>(dims.channels) * pixel_size;
  image_data.resize(num_bytes);

  // Process image in tiles, copying directly to final positions
  // This approach is cache-efficient and processes tiles row-by-row
  for (int y = 0; y < dims.GetHeight(); y += tile_height) {
    for (int x = 0; x < dims.GetWidth(); x += tile_width) {
      int actual_width = std::min(tile_width, dims.GetWidth() - x);
      int actual_height = std::min(tile_height, dims.GetHeight() - y);

      // Get tile from input
      Tile tile = input.GetTile(x, y, actual_width, actual_height);

      // Copy tile data to appropriate position in full image
      // Using visitor to handle different pixel types
      std::visit(
          [&](const auto& tile_data) {
            using T = typename std::decay_t<decltype(tile_data)>::value_type;
            const uint8_t* tile_bytes =
                reinterpret_cast<const uint8_t*>(tile_data.data());

            for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
              size_t src_row_start =
                  tile_y * tile.width * dims.channels * sizeof(T);
              size_t dst_row_start = ((y + tile_y) * dims.GetWidth() + x) *
                                     dims.channels * sizeof(T);
              std::copy_n(&tile_bytes[src_row_start],
                          tile.width * dims.channels * sizeof(T),
                          &image_data[dst_row_start]);
            }
          },
          tile.GetVariantData());
    }
  }
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SINKS_LODEPNG_PNG_SINK_H_

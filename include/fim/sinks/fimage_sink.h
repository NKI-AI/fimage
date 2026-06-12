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
 * @file fimage_sink.h
 * @brief FImage native format sink implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the FImageSink class which provides writing capabilities
 * for the custom FImage format. The format features:
 * - Fixed 256-byte header with complete metadata
 * - Optional tiling with efficient seek
 * - Multiple compression options (None, LZ4, Zstd)
 * - Per-tile compression for parallel processing
 * - Seek table at end of file for compressed formats
 * - Support for various pixel types and layouts
 *
 * Format version: 1
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SINKS_FIMAGE_SINK_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SINKS_FIMAGE_SINK_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "fim/pipeline.h"
#include "fim/types.h"
#include "fim/utilities/compressor.h"

namespace fs = std::filesystem;

namespace fim {

/**
 * @brief Fixed-size header structure for FImage format (256 bytes).
 *
 * This structure defines the binary layout of the FImage file header.
 * All multi-byte integers are stored in little-endian format.
 * The header is exactly 256 bytes to be cache-line friendly.
 */
struct FImageHeader {
  std::array<char, 8> magic;       ///< "FIMAGE\x01\x00" (includes version)
  std::uint32_t width;             ///< Image width in pixels
  std::uint32_t height;            ///< Image height in pixels
  std::uint32_t channels;          ///< Number of channels
  std::uint8_t pixel_type;         ///< Pixel data type (PixelType enum)
  std::uint8_t data_layout;        ///< Data layout (DataLayout enum)
  std::uint8_t compression;        ///< Compression type (CompressionType enum)
  std::uint8_t quantization_mode;  ///< Reserved for future (0 for now)
  std::uint32_t tile_width;        ///< Tile width (0 = contiguous)
  std::uint32_t tile_height;       ///< Tile height (0 = contiguous)
  std::uint32_t num_tiles_x;       ///< Number of tiles horizontally
  std::uint32_t num_tiles_y;       ///< Number of tiles vertically
  double mpp_x;                    ///< Microns per pixel X (0.0 = unknown)
  double mpp_y;                    ///< Microns per pixel Y (0.0 = unknown)
  std::uint64_t data_offset;       ///< Offset to tile data (always 256)
  std::uint64_t
      seek_table_offset;  ///< Offset to seek table (0 if uncompressed)
  std::array<std::uint8_t, 184> reserved;  ///< Reserved for future extensions

  /**
   * @brief Constructs a default header.
   */
  FImageHeader();

  /**
   * @brief Writes the header to a binary stream in little-endian format.
   *
   * @param out Output stream to write to
   */
  void WriteTo(std::ostream& out) const;

  /**
   * @brief Reads the header from a binary stream.
   *
   * @param in Input stream to read from
   * @return true if successful, false otherwise
   */
  bool ReadFrom(std::istream& in);

  /**
   * @brief Validates the header magic and version.
   *
   * @return true if valid, false otherwise
   */
  bool IsValid() const;

  /**
   * @brief Gets the size of a single pixel in bytes.
   *
   * @return Size in bytes based on pixel_type
   */
  size_t GetPixelSize() const;
};

/**
 * @brief Seek table entry for compressed tiles.
 *
 * Each entry stores the location and size of a compressed tile.
 * Entries are written in row-major order (left-to-right, top-to-bottom).
 */
struct SeekTableEntry {
  std::uint64_t offset;           ///< Absolute file offset to tile data
  std::uint64_t compressed_size;  ///< Size of compressed tile in bytes

  /**
   * @brief Writes the entry to a binary stream in little-endian format.
   *
   * @param out Output stream to write to
   */
  void WriteTo(std::ostream& out) const;

  /**
   * @brief Reads the entry from a binary stream.
   *
   * @param in Input stream to read from
   * @return true if successful, false otherwise
   */
  bool ReadFrom(std::istream& in);
};

/**
 * @brief FImage format sink for writing native FImage files.
 *
 * This class provides a high-performance sink for the custom FImage format.
 * It supports:
 * - Tiled and contiguous writing modes
 * - Multiple compression algorithms (None, LZ4, Zstd)
 * - Per-tile compression for parallelization
 * - Efficient single-pass writing with seek table at end
 * - Metadata preservation (MPP, pixel type, layout)
 *
 * The class follows the CRTP pattern by inheriting from SinkBase.
 *
 * @note Use Create() factory method to instantiate.
 */
class FImageSink : public SinkBase<FImageSink> {
 public:
  /**
   * @brief Creates an FImage sink with default settings (factory method).
   *
   * Uses contiguous (non-tiled) mode with no compression.
   *
   * @param filename Path to the output FImage file
   * @return FImageSink instance
   */
  static FImageSink Create(const fs::path& filename);

  /**
   * @brief Creates an FImage sink with tiling (factory method).
   *
   * @param filename Path to the output FImage file
   * @param tile_size Tile size for tiled output
   * @param compression Compression type (default: None)
   * @return FImageSink instance
   */
  static FImageSink Create(
      const fs::path& filename, const TileSize& tile_size,
      CompressionType compression = CompressionType::kNone);

  /**
   * @brief Creates an FImage sink with full configuration (factory method).
   *
   * @param filename Path to the output FImage file
   * @param tile_size Tile size (set to {0, 0} for contiguous mode)
   * @param compression Compression type
   * @param mpp_x Microns per pixel X (0.0 = unknown)
   * @param mpp_y Microns per pixel Y (0.0 = unknown)
   * @return FImageSink instance
   */
  static FImageSink Create(const fs::path& filename, const TileSize& tile_size,
                           CompressionType compression, double mpp_x,
                           double mpp_y);

  /**
   * @brief Default destructor.
   */
  ~FImageSink() = default;

  /**
   * @brief Renders the input to an FImage file.
   *
   * This method processes the input and creates an FImage file with the
   * configured settings (tiling, compression, metadata).
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to render
   * @throw std::runtime_error if the file cannot be created or written
   */
  template <typename InputType>
  void Render(const InputType& input);

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param filename Path to the output FImage file
   * @param tile_size Tile size (0,0 for contiguous)
   * @param compression Compression type
   * @param mpp_x Microns per pixel X
   * @param mpp_y Microns per pixel Y
   */
  explicit FImageSink(const fs::path& filename, const TileSize& tile_size,
                      CompressionType compression, double mpp_x, double mpp_y);

  /**
   * @brief Renders in tiled mode.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to render
   */
  template <typename InputType>
  void RenderTiled(const InputType& input);

  /**
   * @brief Renders in contiguous mode.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to render
   */
  template <typename InputType>
  void RenderContiguous(const InputType& input);

  /**
   * @brief Assembles the full image into a contiguous buffer.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to assemble
   * @return TileData variant containing the assembled image data in the
   * appropriate type
   */
  template <typename InputType>
  TileData AssembleImageData(const InputType& input);

  /**
   * @brief Creates a header from image dimensions and settings.
   *
   * @param dims Image dimensions
   * @return Initialized FImageHeader
   */
  [[nodiscard]] FImageHeader CreateHeader(const ImageInfo& dims) const;

  TileSize tile_size_;           ///< Tile size (0,0 = contiguous)
  CompressionType compression_;  ///< Compression algorithm
  double mpp_x_;                 ///< Microns per pixel X
  double mpp_y_;                 ///< Microns per pixel Y
};

// Template implementations

template <typename InputType>
void FImageSink::Render(const InputType& input) {
  if (tile_size_.width > 0 && tile_size_.height > 0) {
    RenderTiled(input);
  } else {
    RenderContiguous(input);
  }
}

template <typename InputType>
void FImageSink::RenderTiled(const InputType& input) {
  auto dims = input.GetDimensions();
  FImageHeader header = CreateHeader(dims);

  // Calculate number of tiles
  size_t num_tiles_x =
      (dims.GetWidth() + tile_size_.width - 1) / tile_size_.width;
  size_t num_tiles_y =
      (dims.GetHeight() + tile_size_.height - 1) / tile_size_.height;
  header.num_tiles_x = static_cast<std::uint32_t>(num_tiles_x);
  header.num_tiles_y = static_cast<std::uint32_t>(num_tiles_y);

  // Build list of tile coordinates
  struct TileCoord {
    int x;
    int y;
    int width;
    int height;
  };

  std::vector<TileCoord> tile_coords;
  tile_coords.reserve(num_tiles_x * num_tiles_y);

  for (size_t tile_y = 0; tile_y < num_tiles_y; ++tile_y) {
    for (size_t tile_x = 0; tile_x < num_tiles_x; ++tile_x) {
      int x = static_cast<int>(tile_x * tile_size_.width);
      int y = static_cast<int>(tile_y * tile_size_.height);
      int actual_width = std::min(tile_size_.width, dims.GetWidth() - x);
      int actual_height = std::min(tile_size_.height, dims.GetHeight() - y);
      tile_coords.push_back({x, y, actual_width, actual_height});
    }
  }

  // Phase 1: Parallel compression of all tiles
  // Preallocate storage for compressed tiles (preserves order)
  std::vector<std::vector<uint8_t>> compressed_tiles(tile_coords.size());

  // Check if we should parallelize
  if (ShouldParallelize(tile_coords.size())) {
    auto& pool = GetThreadPool();

    // Parallel processing: compress tiles concurrently
    auto futures = pool.submit_blocks(
        0, tile_coords.size(),
        [&input, &tile_coords, &compressed_tiles, this](size_t start,
                                                        size_t end) {
          for (size_t i = start; i < end; ++i) {
            const auto& coord = tile_coords[i];

            // Get tile from input
            Tile tile =
                input.GetTile(coord.x, coord.y, coord.width, coord.height);

            // Pad tile to full tile size
            tile.expected_width = tile_size_.width;
            tile.expected_height = tile_size_.height;
            tile.needs_padding = (tile.width < tile_size_.width ||
                                  tile.height < tile_size_.height);
            std::vector<uint8_t> tile_data = tile.CreatePaddedDataAsBytes();

            // Compress if needed (CPU-bound work done in parallel)
            auto compress_result = CompressData(tile_data, compression_);
            if (!compress_result.ok()) {
              throw std::runtime_error(
                  "Compression failed: " +
                  std::string(compress_result.status().message()));
            }

            // Store compressed data at correct index (preserves order)
            compressed_tiles[i] = std::move(*compress_result);
          }
        });

    // Wait for all compression tasks to complete and propagate exceptions
    for (auto& future : futures) {
      future.get();  // Use get() to surface exceptions
    }
  } else {
    // Serial path for small images
    for (size_t i = 0; i < tile_coords.size(); ++i) {
      const auto& coord = tile_coords[i];

      // Get tile from input
      Tile tile = input.GetTile(coord.x, coord.y, coord.width, coord.height);

      // Pad tile to full tile size
      tile.expected_width = tile_size_.width;
      tile.expected_height = tile_size_.height;
      tile.needs_padding =
          (tile.width < tile_size_.width || tile.height < tile_size_.height);
      std::vector<uint8_t> tile_data = tile.CreatePaddedDataAsBytes();

      // Compress if needed
      auto compress_result = CompressData(tile_data, compression_);
      if (!compress_result.ok()) {
        throw std::runtime_error(
            "Compression failed: " +
            std::string(compress_result.status().message()));
      }
      compressed_tiles[i] = std::move(*compress_result);
    }
  }

  // Phase 2: Sequential write (preserves file order, maintains seek table
  // correctness)
  std::ofstream out(filename_, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Failed to create FImage file: " +
                             filename_.string());
  }

  // Write placeholder header (will update seek_table_offset later)
  header.WriteTo(out);

  // Track seek table entries for compressed mode
  std::vector<SeekTableEntry> seek_table;
  if (compression_ != CompressionType::kNone) {
    seek_table.reserve(tile_coords.size());
  }

  // Write all compressed tiles sequentially
  for (size_t i = 0; i < compressed_tiles.size(); ++i) {
    const auto& output_data = compressed_tiles[i];

    // Record position and size
    std::uint64_t tile_offset = out.tellp();
    std::uint64_t tile_size = output_data.size();

    if (compression_ != CompressionType::kNone) {
      SeekTableEntry entry;
      entry.offset = tile_offset;
      entry.compressed_size = tile_size;
      seek_table.push_back(entry);
    }

    // Write tile data
    out.write(reinterpret_cast<const char*>(output_data.data()),
              output_data.size());
  }

  // Write seek table if compressed
  if (compression_ != CompressionType::kNone) {
    std::uint64_t seek_table_offset = out.tellp();

    // Write all seek table entries
    for (const auto& entry : seek_table) {
      entry.WriteTo(out);
    }

    // Update header with seek table offset
    out.seekp(0);
    header.seek_table_offset = seek_table_offset;
    header.WriteTo(out);
  }

  out.close();
}

template <typename InputType>
void FImageSink::RenderContiguous(const InputType& input) {
  auto dims = input.GetDimensions();
  FImageHeader header = CreateHeader(dims);

  // Open output file
  std::ofstream out(filename_, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Failed to create FImage file: " +
                             filename_.string());
  }

  // Write placeholder header
  header.WriteTo(out);

  // Assemble full image
  TileData tile_data = AssembleImageData(input);

  // Convert TileData to bytes for compression
  std::vector<uint8_t> image_data = std::visit(
      [](const auto& vec) -> std::vector<uint8_t> {
        const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(vec.data());
        size_t byte_size =
            vec.size() *
            sizeof(typename std::decay_t<decltype(vec)>::value_type);
        return std::vector<uint8_t>(data_ptr, data_ptr + byte_size);
      },
      tile_data);

  // Compress if needed
  auto compress_result = CompressData(image_data, compression_);
  if (!compress_result.ok()) {
    throw std::runtime_error("Compression failed: " +
                             std::string(compress_result.status().message()));
  }
  std::vector<uint8_t> output_data = std::move(*compress_result);

  // Record position if compressed
  if (compression_ != CompressionType::kNone) {
    std::uint64_t data_offset = out.tellp();
    std::uint64_t data_size = output_data.size();

    // Write data
    out.write(reinterpret_cast<const char*>(output_data.data()), data_size);

    // Write seek table with single entry
    std::uint64_t seek_table_offset = out.tellp();
    SeekTableEntry entry;
    entry.offset = data_offset;
    entry.compressed_size = data_size;
    entry.WriteTo(out);

    // Update header
    out.seekp(0);
    header.seek_table_offset = seek_table_offset;
    header.WriteTo(out);
  } else {
    // Uncompressed: just write data
    out.write(reinterpret_cast<const char*>(output_data.data()),
              output_data.size());
  }

  out.close();
}

template <typename InputType>
TileData FImageSink::AssembleImageData(const InputType& input) {
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

  // Helper lambda to assemble tiles into a typed vector
  auto assemble_typed = [&]<typename T>() -> TileData {
    size_t num_elements = static_cast<size_t>(dims.GetWidth()) *
                          static_cast<size_t>(dims.GetHeight()) *
                          static_cast<size_t>(dims.channels);
    std::vector<T> image_data(num_elements);

    // Process image in tiles
    for (int y = 0; y < dims.GetHeight(); y += tile_height) {
      for (int x = 0; x < dims.GetWidth(); x += tile_width) {
        int actual_width = std::min(tile_width, dims.GetWidth() - x);
        int actual_height = std::min(tile_height, dims.GetHeight() - y);

        // Get tile from input
        Tile tile = input.GetTile(x, y, actual_width, actual_height);
        const auto& tile_data = std::get<std::vector<T>>(tile.GetVariantData());

        if (dims.layout == DataLayout::kChannelsLast) {
          // Copy tile data to appropriate position (HWC)
          for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
            const T* src_row =
                tile_data.data() + static_cast<size_t>(tile_y) *
                                       static_cast<size_t>(tile.width) *
                                       static_cast<size_t>(dims.channels);
            T* dst_row =
                image_data.data() + (static_cast<size_t>(y + tile_y) *
                                         static_cast<size_t>(dims.GetWidth()) +
                                     static_cast<size_t>(x)) *
                                        static_cast<size_t>(dims.channels);
            std::copy(src_row,
                      src_row + static_cast<size_t>(tile.width) *
                                    static_cast<size_t>(dims.channels),
                      dst_row);
          }
        } else {
          // Channels-first (CHW)
          for (int c = 0; c < dims.channels; ++c) {
            for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
              const T* src_row =
                  tile_data.data() +
                  (static_cast<size_t>(c) * static_cast<size_t>(tile.height) +
                   static_cast<size_t>(tile_y)) *
                      static_cast<size_t>(tile.width);
              T* dst_row = image_data.data() +
                           (static_cast<size_t>(c) *
                                static_cast<size_t>(dims.GetHeight()) +
                            static_cast<size_t>(y + tile_y)) *
                               static_cast<size_t>(dims.GetWidth()) +
                           static_cast<size_t>(x);
              std::copy(src_row, src_row + static_cast<size_t>(tile.width),
                        dst_row);
            }
          }
        }
      }
    }

    return TileData(std::move(image_data));
  };

  // Dispatch based on pixel type
  switch (dims.pixel_type) {
    case PixelType::kUInt8:
      return assemble_typed.template operator()<uint8_t>();
    case PixelType::kUInt16:
      return assemble_typed.template operator()<uint16_t>();
    case PixelType::kFloat32:
      return assemble_typed.template operator()<float>();
    default:
      throw std::runtime_error("Unsupported pixel type in AssembleImageData");
  }
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SINKS_FIMAGE_SINK_H_

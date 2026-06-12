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
 * @file fimage_source.h
 * @brief FImage native format source implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the FImageSource class which provides reading capabilities
 * for the custom FImage format. It supports:
 * - Tiled and contiguous reading modes
 * - Multiple compression algorithms (None, LZ4, Zstd)
 * - Efficient random tile access via seek table
 * - Thread-safe file operations
 * - Lazy decompression on-demand
 *
 * Format version: 1
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SOURCES_FIMAGE_SOURCE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SOURCES_FIMAGE_SOURCE_H_

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <vector>

#include "fim/pipeline.h"
#include "fim/sinks/fimage_sink.h"  // For FImageHeader and SeekTableEntry
#include "fim/types.h"

namespace fim {

namespace fs = std::filesystem;

/**
 * @brief FImage format source for reading native FImage files.
 *
 * This class provides a source implementation for FImage files with support
 * for both tiled and contiguous storage modes, compression, and efficient
 * random access. The implementation uses lazy loading and decompression to
 * minimize memory usage.
 *
 * The class follows the CRTP pattern by inheriting from SourceBase and
 * provides thread-safe tile access through mutex-protected file operations.
 *
 * Key Features:
 * - Efficient tile-based random access
 * - On-demand decompression (LZ4, Zstd)
 * - Thread-safe file operations
 * - Support for tiled and contiguous modes
 * - Move-only semantics for resource management
 *
 * @note Use Create() factory method to instantiate.
 */
class FImageSource : public SourceBase<FImageSource> {
 public:
  /**
   * @brief Creates an FImage source from a file path (factory method).
   *
   * This is the only way to create an FImageSource instance. The method
   * validates the file, reads the header, and loads the seek table if needed.
   *
   * @param filename Path to the FImage file to read
   * @return FImageSource instance
   * @throw std::runtime_error if the file doesn't exist, is invalid, or
   *        cannot be read
   *
   * @note Future: This will be migrated to return absl::StatusOr<FImageSource>
   */
  static FImageSource Create(const fs::path& filename);

  /**
   * @brief Destructor that closes the file handle.
   */
  ~FImageSource();

  /**
   * @brief Deleted copy constructor.
   *
   * FImage sources cannot be copied to avoid issues with file handle
   * management and to ensure clear ownership semantics.
   */
  FImageSource(const FImageSource&) = delete;

  /**
   * @brief Deleted copy assignment operator.
   *
   * FImage sources cannot be copied to avoid issues with file handle
   * management and to ensure clear ownership semantics.
   */
  FImageSource& operator=(const FImageSource&) = delete;

  /**
   * @brief Move constructor.
   *
   * Transfers ownership of the FImage source and its file handle to the
   * new instance. The source object is left in a valid but unspecified state.
   *
   * @param other FImage source to move from
   */
  FImageSource(FImageSource&& other) noexcept;

  /**
   * @brief Move assignment operator.
   *
   * Transfers ownership of the FImage source and its file handle to this
   * instance. The source object is left in a valid but unspecified state.
   *
   * @param other FImage source to move from
   * @return Reference to this instance
   */
  FImageSource& operator=(FImageSource&& other) noexcept;

  /**
   * @brief Gets the dimensions of the image.
   *
   * @return ImageInfo containing width, height, channels, and layout
   */
  ImageInfo GetDimensions() const;

  /**
   * @brief Gets the number of channels in the image.
   *
   * @return Number of color channels
   */
  int GetChannels() const;

  /**
   * @brief Gets the memory layout of the image data.
   *
   * @return DataLayout specifying the memory organization
   */
  DataLayout GetMemoryLayout() const;

  /**
   * @brief Gets the ideal tile size for processing.
   *
   * Returns the tile size from the file header for tiled files, or a
   * default size for contiguous files.
   *
   * @return TileSize with the ideal dimensions for tile requests
   */
  TileSize GetIdealTileSize() const;

  /**
   * @brief Gets a tile of image data.
   *
   * Retrieves a rectangular region of the image. For tiled files, this
   * may involve reading and decompressing one or more tiles. For contiguous
   * files, this extracts the region from the image data.
   *
   * Thread-safe: Multiple threads can call this concurrently.
   *
   * @param x X coordinate of the top-left corner
   * @param y Y coordinate of the top-left corner
   * @param width Width of the requested region
   * @param height Height of the requested region
   * @return Tile containing the requested image data
   * @throw std::runtime_error if the region is out of bounds or read fails
   */
  Tile GetTile(int x, int y, int width, int height) const;

  /**
   * @brief Gets the MPP (microns per pixel) in X direction.
   *
   * @return MPP X value, or 0.0 if unknown
   */
  double GetMppX() const { return header_.mpp_x; }

  /**
   * @brief Gets the MPP (microns per pixel) in Y direction.
   *
   * @return MPP Y value, or 0.0 if unknown
   */
  double GetMppY() const { return header_.mpp_y; }

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param filename Path to the FImage file
   */
  explicit FImageSource(const fs::path& filename);

  /**
   * @brief Reads and validates the file header.
   *
   * @throw std::runtime_error if header is invalid or read fails
   */
  void ReadHeader();

  /**
   * @brief Loads the seek table from the end of the file.
   *
   * Only called for compressed files. Reads the seek table entries
   * that map tile indices to file offsets and sizes.
   *
   * @throw std::runtime_error if seek table is invalid or read fails
   */
  void LoadSeekTable();

  /**
   * @brief Reads a single tile's data from a tiled file.
   *
   * Calculates the tile index, reads from file (using seek table if
   * compressed), and decompresses if needed.
   *
   * @param tile_x Tile column index
   * @param tile_y Tile row index
   * @return Tile data (full tile size, padded if necessary)
   * @throw std::runtime_error if read or decompression fails
   */
  std::vector<uint8_t> ReadTileData(int tile_x, int tile_y) const;

  /**
   * @brief Reads a region from a contiguous file.
   *
   * Reads the entire image if compressed (decompresses), then extracts
   * the requested region. For uncompressed, reads only the needed region.
   *
   * @param x X coordinate of the region
   * @param y Y coordinate of the region
   * @param width Width of the region
   * @param height Height of the region
   * @return Region data
   * @throw std::runtime_error if read or decompression fails
   */
  std::vector<uint8_t> ReadContiguousRegion(int x, int y, int width,
                                            int height) const;

  /**
   * @brief Extracts a region from full image data.
   *
   * Helper method to extract a rectangular region from a full image buffer.
   *
   * @param full_data Complete image data
   * @param x X coordinate of the region
   * @param y Y coordinate of the region
   * @param width Width of the region
   * @param height Height of the region
   * @return Extracted region data
   */
  std::vector<uint8_t> ExtractRegion(const std::vector<uint8_t>& full_data,
                                     int x, int y, int width, int height) const;

  fs::path filename_;                            ///< Path to the FImage file
  mutable std::unique_ptr<std::ifstream> file_;  ///< File handle
  mutable std::mutex file_mutex_;                ///< Protects file operations

  FImageHeader header_;                     ///< Parsed file header
  std::vector<SeekTableEntry> seek_table_;  ///< Seek table for compressed files
  ImageInfo dimensions_;                    ///< Image dimensions
  bool is_tiled_;                           ///< True if tiled mode
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SOURCES_FIMAGE_SOURCE_H_

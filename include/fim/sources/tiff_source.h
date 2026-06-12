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
 * @file tiff_source.h
 * @brief TIFF image source implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the TiffSource class which provides TIFF image reading
 * capabilities for the fim image processing pipeline. It uses libtiff for
 * TIFF decoding and supports both tiled and strip-based TIFF files with
 * efficient tile-based access.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SOURCES_TIFF_SOURCE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SOURCES_TIFF_SOURCE_H_

#include <tiffio.h>

#include <filesystem>
#include <mutex>
#include <vector>

#include "fim/pipeline.h"
#include "fim/types.h"

namespace fim {

namespace fs = std::filesystem;

/**
 * @brief TIFF image source for reading TIFF files.
 *
 * This class provides a source implementation for TIFF image files using
 * libtiff. It supports both tiled and strip-based TIFF files and provides
 * efficient tile-based access to the image content. The class automatically
 * detects the TIFF structure and optimizes access patterns accordingly.
 * This is a simple tiff source and only reads the first page of the TIFF file.
 *
 * The class follows the CRTP pattern by inheriting from SourceBase and
 * provides the required interface methods for image sources. It uses lazy
 * loading for image metadata and keeps the TIFF file handle open for
 * efficient random access.
 *
 * @note Use Create() factory method to instantiate.
 */
class TiffSource : public SourceBase<TiffSource> {
 public:
  /**
   * @brief Creates a TIFF source from a file path (factory method).
   *
   * This is the only way to create a TiffSource instance. The method validates
   * that the file exists before constructing the source.
   *
   * @param filename Path to the TIFF file to read
   * @return TiffSource instance
   * @throw std::runtime_error if the TIFF file does not exist or cannot be
   * opened
   *
   * @note Future: This will be migrated to return absl::StatusOr<TiffSource>
   * to avoid exceptions.
   */
  static TiffSource Create(const fs::path& filename);
  /**
   * @brief Creates a TIFF source from a file path and directory index (page).
   *
   * This overload selects a specific TIFF directory (a.k.a. page/IFD) before
   * reading tags or pixel data. Directory index 0 is the first page.
   *
   * @param filename Path to the TIFF file to read
   * @param directory_index TIFF directory (page) index to read (0-based)
   * @return TiffSource instance
   * @throw std::runtime_error if the TIFF file does not exist or cannot be
   * opened, or if the directory does not exist
   */
  static TiffSource Create(const fs::path& filename, int directory_index);

  /**
   * @brief Destructor that closes the TIFF file handle.
   */
  ~TiffSource();

  /**
   * @brief Deleted copy constructor.
   *
   * TIFF sources cannot be copied to avoid issues with file handle
   * management and to ensure clear ownership semantics.
   */
  TiffSource(const TiffSource&) = delete;

  /**
   * @brief Deleted copy assignment operator.
   *
   * TIFF sources cannot be copied to avoid issues with file handle
   * management and to ensure clear ownership semantics.
   */
  TiffSource& operator=(const TiffSource&) = delete;

  /**
   * @brief Move constructor.
   *
   * Transfers ownership of the TIFF source and its file handle to the
   * new instance. The source object is left in a valid but unspecified state.
   *
   * @param other TIFF source to move from
   */
  TiffSource(TiffSource&& other) noexcept;

  /**
   * @brief Move assignment operator.
   *
   * Transfers ownership of the TIFF source and its file handle to this
   * instance. The source object is left in a valid but unspecified state.
   *
   * @param other TIFF source to move from
   * @return Reference to this instance
   */
  TiffSource& operator=(TiffSource&& other) noexcept;

  /**
   * @brief Gets the dimensions of the TIFF image.
   *
   * This method returns the complete dimensions of the TIFF image including
   * width, height, and number of channels. The information is read from
   * the TIFF file headers and cached for subsequent calls.
   *
   * @return ImageInfo containing the TIFF image dimensions
   * @throw std::runtime_error if the TIFF file is not properly opened
   */
  ImageInfo GetDimensions() const;

  /**
   * @brief Gets the ideal tile size for processing this TIFF.
   *
   * For tiled TIFF files, this returns the actual tile dimensions used
   * in the file. For strip-based TIFF files, this returns the strip
   * dimensions (full width, strip height). This information is used
   * to optimize processing performance.
   *
   * @return TileSize containing the ideal tile dimensions
   */
  TileSize GetIdealTileSize() const;

  /**
   * @brief Gets a tile from the TIFF image.
   *
   * This method retrieves a rectangular region of the TIFF image as a tile.
   * The tile coordinates are clamped to the image bounds. The method
   * automatically handles both tiled and strip-based TIFF files.
   *
   * @param x X coordinate of the top-left corner of the tile
   * @param y Y coordinate of the top-left corner of the tile
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile containing the requested TIFF image data
   * @throw std::runtime_error if the TIFF file is not properly opened or data
   * cannot be read
   */
  Tile GetTile(int x, int y, int width, int height) const;

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * The constructor opens the TIFF file and initializes the source but
   * doesn't immediately load the image metadata. The actual loading is
   * performed lazily when image dimensions or tiles are first requested.
   *
   * @param filename Path to the TIFF file to read
   * @throw std::runtime_error if the TIFF file cannot be opened
   */
  explicit TiffSource(const fs::path& filename, int directory_index);

  /**
   * @brief Opens the TIFF file and initializes the handle.
   *
   * @param filename Path to the TIFF file to open
   * @throw std::runtime_error if the TIFF file cannot be opened
   */
  void OpenTiff(const fs::path& filename);

  /**
   * @brief Closes the TIFF file handle.
   */
  void CloseTiff();

  fs::path filename_;                 ///< Path to the TIFF file
  int directory_index_ = 0;           ///< TIFF directory (page) index (0-based)
  TIFF* tiff_handle_;                 ///< LibTIFF file handle for metadata
  mutable ImageInfo dimensions_;      ///< Cached image dimensions
  mutable TileSize ideal_tile_size_;  ///< Cached ideal tile size
  mutable bool dimensions_loaded_;    ///< Whether dimensions have been loaded
  mutable std::mutex tiff_mutex_;  ///< Mutex to protect metadata loading (not
                                   ///< thread-safe)

  // Handle pool for parallel reads
  mutable std::vector<TIFF*> handle_pool_;  ///< Pool of read-only TIFF handles
  mutable std::mutex pool_mutex_;  ///< Mutex to protect handle pool operations

  /**
   * @brief Loads the TIFF image dimensions and metadata.
   *
   * This method is called lazily when image metadata is first needed.
   * It reads the TIFF file headers and determines the optimal tile size
   * based on the file structure.
   *
   * @throw std::runtime_error if the TIFF file is not properly opened
   */
  void LoadDimensions() const;

  /**
   * @brief Reads tile data from a tiled TIFF file.
   *
   * This method handles reading data from TIFF files that use the
   * tiled format. It reads the necessary tiles and copies the relevant
   * portions to the output tile.
   *
   * @param tile Reference to the tile to populate with data
   * @param handle TIFF handle to use for reading (from pool)
   * @throw std::runtime_error if tile data cannot be read
   */
  void ReadTiledData(Tile& tile, TIFF* handle) const;

  /**
   * @brief Reads strip data from a strip-based TIFF file.
   *
   * This method handles reading data from TIFF files that use the
   * strip format. It reads the necessary strips and copies the relevant
   * portions to the output tile.
   *
   * @param tile Reference to the tile to populate with data
   * @param handle TIFF handle to use for reading (from pool)
   * @throw std::runtime_error if strip data cannot be read
   */
  void ReadStripData(Tile& tile, TIFF* handle) const;

  /**
   * @brief Copies data from a source tile to the output tile.
   *
   * This helper method handles the intersection and copying of data
   * between a source tile from the TIFF file and the requested output tile.
   *
   * @param src_data Source tile data from the TIFF file
   * @param tile Output tile to populate
   * @param tile_x X coordinate of the source tile
   * @param tile_y Y coordinate of the source tile
   * @param tile_width Width of the source tile
   * @param tile_height Height of the source tile
   */
  void CopyTileData(const std::vector<uint8_t>& src_data,
                    std::vector<uint8_t>& dst_data, const Tile& tile,
                    int tile_x, int tile_y, int tile_width,
                    int tile_height) const;

  /**
   * @brief Copies data from a source strip to the output buffer.
   *
   * This helper method handles the intersection and copying of data
   * between a source strip from the TIFF file and the requested output buffer.
   *
   * @param src_data Source strip data from the TIFF file (in bytes)
   * @param dst_data Destination buffer for output tile (in bytes)
   * @param tile Output tile dimensions
   * @param strip_row Starting row of the source strip
   * @param rows_per_strip Number of rows per strip
   */
  void CopyStripData(const std::vector<uint8_t>& src_data,
                     std::vector<uint8_t>& dst_data, const Tile& tile,
                     int strip_row, int rows_per_strip) const;

  /**
   * @brief Acquires a TIFF handle from the pool for reading.
   *
   * This method retrieves a read-only TIFF handle from the pool, enabling
   * parallel tile reads. If no handle is available, it creates a new one.
   * The handle must be returned via ReleaseHandle() when done.
   *
   * @return TIFF* handle for reading
   * @throw std::runtime_error if a new handle cannot be opened
   */
  TIFF* AcquireHandle() const;

  /**
   * @brief Returns a TIFF handle to the pool for reuse.
   *
   * @param handle TIFF handle to return to the pool
   */
  void ReleaseHandle(TIFF* handle) const;

  /**
   * @brief Initializes the handle pool with read-only handles.
   *
   * Creates multiple read-only TIFF handles for parallel access.
   * Called during construction.
   */
  void InitializeHandlePool();
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SOURCES_TIFF_SOURCE_H_

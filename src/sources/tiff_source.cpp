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
 * @file tiff_source.cpp
 * @brief Implementation of TIFF image source for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the implementation of the TiffSource class which provides
 * TIFF image reading capabilities using libtiff. It handles both tiled and
 * strip-based TIFF files with efficient tile-based access and lazy loading
 * of image metadata.
 */
#include "fim/sources/tiff_source.h"

#include <tiffio.h>

#include <algorithm>
#include <cstring>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "aifocore/platform/portability.h"
#include "fim/utilities/buffer_pool.h"
#include "fim/utilities/tile_helpers.h"

namespace fim {

namespace fs = std::filesystem;

namespace {

inline tsize_t TiffClientRead(thandle_t handle, tdata_t data, tsize_t size) {
  FILE* file = reinterpret_cast<FILE*>(handle);
  return static_cast<tsize_t>(
      aifocore::portable_fread(data, static_cast<size_t>(size), file));
}

inline tsize_t TiffClientWrite(thandle_t handle, tdata_t data, tsize_t size) {
  FILE* file = reinterpret_cast<FILE*>(handle);
  return static_cast<tsize_t>(
      aifocore::portable_fwrite(data, static_cast<size_t>(size), file));
}

inline toff_t TiffClientSeek(thandle_t handle, toff_t off, int whence) {
  FILE* file = reinterpret_cast<FILE*>(handle);
  if (aifocore::portable_fseek(file, static_cast<int64_t>(off), whence) != 0) {
    return static_cast<toff_t>(-1);
  }
  return static_cast<toff_t>(aifocore::portable_ftell(file));
}

inline int TiffClientClose(thandle_t handle) {
  FILE* file = reinterpret_cast<FILE*>(handle);
  return aifocore::portable_fclose(file);
}

inline toff_t TiffClientSize(thandle_t handle) {
  FILE* file = reinterpret_cast<FILE*>(handle);
  const int64_t size = aifocore::portable_filesize(file);
  if (size < 0) {
    return static_cast<toff_t>(0);
  }
  return static_cast<toff_t>(size);
}

inline int TiffClientMap(thandle_t, tdata_t*, toff_t*) {
  return 0;
}

inline void TiffClientUnmap(thandle_t, tdata_t, toff_t) {}

inline TIFF* OpenTiffForRead(const fs::path& filename, const char* mode) {
  FILE* file = aifocore::portable_fopen(filename, "rb");
  if (file == nullptr) {
    return nullptr;
  }
  const std::string name = filename.string();
  TIFF* tiff = TIFFClientOpen(name.c_str(), mode,
                              reinterpret_cast<thandle_t>(file), TiffClientRead,
                              TiffClientWrite, TiffClientSeek, TiffClientClose,
                              TiffClientSize, TiffClientMap, TiffClientUnmap);
  if (tiff == nullptr) {
    aifocore::portable_fclose(file);
    return nullptr;
  }
  return tiff;
}

}  // namespace

TiffSource TiffSource::Create(const fs::path& filename) {
  // Validate file existence before construction
  if (!std::filesystem::exists(filename)) {
    throw std::runtime_error("TIFF file does not exist: " + filename.string());
  }
  return TiffSource(filename, /*directory_index=*/0);
}

TiffSource TiffSource::Create(const fs::path& filename, int directory_index) {
  // Validate file existence before construction
  if (!std::filesystem::exists(filename)) {
    throw std::runtime_error("TIFF file does not exist: " + filename.string());
  }
  if (directory_index < 0) {
    throw std::runtime_error("TIFF directory index must be >= 0");
  }
  return TiffSource(filename, directory_index);
}

TiffSource::TiffSource(const fs::path& filename, int directory_index)
    : filename_(filename),
      directory_index_(directory_index),
      tiff_handle_(nullptr),
      dimensions_loaded_(false) {
  OpenTiff(filename);
  InitializeHandlePool();
}

TiffSource::~TiffSource() {
  CloseTiff();
  // Close all handles in the pool
  std::lock_guard<std::mutex> lock(pool_mutex_);
  for (TIFF* handle : handle_pool_) {
    if (handle) {
      TIFFClose(handle);
    }
  }
  handle_pool_.clear();
}

TiffSource::TiffSource(TiffSource&& other) noexcept
    : filename_(std::move(other.filename_)),
      directory_index_(other.directory_index_),
      tiff_handle_(other.tiff_handle_),
      dimensions_(other.dimensions_),
      ideal_tile_size_(other.ideal_tile_size_),
      dimensions_loaded_(other.dimensions_loaded_),
      handle_pool_(std::move(other.handle_pool_)) {
  other.tiff_handle_ = nullptr;
  other.dimensions_loaded_ = false;
}

TiffSource& TiffSource::operator=(TiffSource&& other) noexcept {
  if (this != &other) {
    CloseTiff();
    // Close existing pool handles
    {
      std::lock_guard<std::mutex> lock(pool_mutex_);
      for (TIFF* handle : handle_pool_) {
        if (handle) {
          TIFFClose(handle);
        }
      }
      handle_pool_.clear();
    }
    filename_ = std::move(other.filename_);
    directory_index_ = other.directory_index_;
    tiff_handle_ = other.tiff_handle_;
    dimensions_ = other.dimensions_;
    ideal_tile_size_ = other.ideal_tile_size_;
    dimensions_loaded_ = other.dimensions_loaded_;
    handle_pool_ = std::move(other.handle_pool_);
    other.tiff_handle_ = nullptr;
    other.dimensions_loaded_ = false;
  }
  return *this;
}

void TiffSource::OpenTiff(const fs::path& filename) {
  tiff_handle_ = OpenTiffForRead(filename, "r");
  if (!tiff_handle_) {
    throw std::runtime_error("Failed to open TIFF file: " + filename.string());
  }
  if (!TIFFSetDirectory(tiff_handle_, static_cast<tdir_t>(directory_index_))) {
    TIFFClose(tiff_handle_);
    tiff_handle_ = nullptr;
    throw std::runtime_error("Failed to set TIFF directory index: " +
                             std::to_string(directory_index_));
  }
}

void TiffSource::CloseTiff() {
  if (tiff_handle_) {
    TIFFClose(tiff_handle_);
    tiff_handle_ = nullptr;
  }
}

ImageInfo TiffSource::GetDimensions() const {
  if (!dimensions_loaded_) {
    LoadDimensions();
  }
  return dimensions_;
}

TileSize TiffSource::GetIdealTileSize() const {
  if (!dimensions_loaded_) {
    LoadDimensions();
  }
  return ideal_tile_size_;
}

void TiffSource::LoadDimensions() const {
  std::lock_guard<std::mutex> lock(tiff_mutex_);

  if (!tiff_handle_) {
    throw std::runtime_error("TIFF file not opened");
  }

  // Double-check dimensions_loaded_ after acquiring lock
  if (dimensions_loaded_) {
    return;
  }

  uint32_t width, height;
  uint16_t samples_per_pixel;
  uint16_t bits_per_sample = 8;                // Default to 8-bit
  uint16_t sample_format = SAMPLEFORMAT_UINT;  // Default to unsigned int

  TIFFGetField(tiff_handle_, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tiff_handle_, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetField(tiff_handle_, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
  TIFFGetFieldDefaulted(tiff_handle_, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
  TIFFGetFieldDefaulted(tiff_handle_, TIFFTAG_SAMPLEFORMAT, &sample_format);

  // Determine pixel type from TIFF tags
  PixelType pixel_type;
  if (sample_format == SAMPLEFORMAT_UINT) {
    if (bits_per_sample == 8) {
      pixel_type = PixelType::kUInt8;
    } else if (bits_per_sample == 16) {
      pixel_type = PixelType::kUInt16;
    } else {
      throw std::runtime_error("Unsupported TIFF bits per sample for UINT: " +
                               std::to_string(bits_per_sample));
    }
  } else if (sample_format == SAMPLEFORMAT_IEEEFP) {
    if (bits_per_sample == 32) {
      pixel_type = PixelType::kFloat32;
    } else {
      throw std::runtime_error("Unsupported TIFF bits per sample for IEEEFP: " +
                               std::to_string(bits_per_sample));
    }
  } else {
    throw std::runtime_error("Unsupported TIFF sample format: " +
                             std::to_string(sample_format));
  }

  dimensions_ = ImageInfo(static_cast<int>(width), static_cast<int>(height),
                          static_cast<int>(samples_per_pixel), pixel_type,
                          DataLayout::kChannelsLast);

  // Check if TIFF is tiled and get tile dimensions
  uint32_t tile_width, tile_height;
  if (TIFFIsTiled(tiff_handle_)) {
    TIFFGetField(tiff_handle_, TIFFTAG_TILEWIDTH, &tile_width);
    TIFFGetField(tiff_handle_, TIFFTAG_TILELENGTH, &tile_height);
    ideal_tile_size_ =
        TileSize(static_cast<int>(tile_width), static_cast<int>(tile_height));
  } else {
    // For non-tiled TIFF, use strip size as ideal tile height
    uint32_t rows_per_strip;
    TIFFGetField(tiff_handle_, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
    ideal_tile_size_ =
        TileSize(static_cast<int>(width), static_cast<int>(rows_per_strip));
  }

  dimensions_loaded_ = true;
}

Tile TiffSource::GetTile(int x, int y, int width, int height) const {
  if (!tiff_handle_) {
    throw std::runtime_error("TIFF file not opened");
  }

  if (!dimensions_loaded_) {
    LoadDimensions();
  }

  // Use shared helper to clamp tile request to image bounds
  auto request =
      tile_helpers::ClampTileRequest(x, y, width, height, dimensions_);

  // Handle case where tile is completely outside image bounds
  if (request.is_empty) {
    return Tile(x, y, 0, 0, dimensions_.channels, width, height,
                dimensions_.layout, dimensions_.pixel_type);
  }

  // Create tile with appropriate padding metadata
  Tile tile = tile_helpers::CreateTileWithPadding(x, y, request.actual_width,
                                                  request.actual_height, width,
                                                  height, dimensions_);

  // Acquire a handle from the pool for parallel reads
  TIFF* handle = AcquireHandle();
  try {
    // Read tile data using the acquired handle
    if (TIFFIsTiled(handle)) {
      ReadTiledData(tile, handle);
    } else {
      ReadStripData(tile, handle);
    }
    // Return handle to pool
    ReleaseHandle(handle);
  } catch (...) {
    // Ensure handle is returned even on exception
    ReleaseHandle(handle);
    throw;
  }

  return tile;
}

void TiffSource::ReadTiledData(Tile& tile, TIFF* handle) const {
  uint32_t tile_width, tile_height;
  TIFFGetField(handle, TIFFTAG_TILEWIDTH, &tile_width);
  TIFFGetField(handle, TIFFTAG_TILELENGTH, &tile_height);

  size_t tile_size = TIFFTileSize(handle);
  // Use buffer pool to reuse scratch buffers across tile reads
  auto& pool = GetBufferPool();
  std::vector<uint8_t> tiff_tile_data = pool.GetBuffer(tile_size);

  // Calculate tile-aligned bounds
  int tile_x_start = (tile.x / tile_width) * tile_width;
  int tile_y_start = (tile.y / tile_height) * tile_height;
  int tile_x_end =
      ((tile.x + tile.width + tile_width - 1) / tile_width) * tile_width;
  int tile_y_end =
      ((tile.y + tile.height + tile_height - 1) / tile_height) * tile_height;

  // Allocate buffer for output tile data (in bytes)
  size_t pixel_size = GetPixelTypeSize(dimensions_.pixel_type);
  size_t num_elements = tile.width * tile.height * tile.channels;
  std::vector<uint8_t> output_data = pool.GetBuffer(num_elements * pixel_size);

  // Read overlapping tiles using tile-aligned coordinates
  for (int ty = tile_y_start; ty < tile_y_end; ty += tile_height) {
    for (int tx = tile_x_start; tx < tile_x_end; tx += tile_width) {
      if (TIFFReadTile(handle, tiff_tile_data.data(), tx, ty, 0, 0) < 0) {
        throw std::runtime_error("Failed to read TIFF tile");
      }

      // Copy relevant portion of tile data to output buffer
      CopyTileData(tiff_tile_data, output_data, tile, tx, ty, tile_width,
                   tile_height);
    }
  }

  // Set tile data using typed interface
  switch (dimensions_.pixel_type) {
    case PixelType::kUInt8:
      tile.SetData(std::move(output_data));
      break;
    case PixelType::kUInt16: {
      // Reinterpret byte buffer as uint16_t vector
      std::vector<uint16_t> typed_data(num_elements);
      std::memcpy(typed_data.data(), output_data.data(),
                  num_elements * sizeof(uint16_t));
      tile.SetData(std::move(typed_data));
      // Return output buffer to pool
      pool.ReturnBuffer(std::move(output_data));
      break;
    }
    case PixelType::kFloat32: {
      // Reinterpret byte buffer as float vector
      std::vector<float> typed_data(num_elements);
      std::memcpy(typed_data.data(), output_data.data(),
                  num_elements * sizeof(float));
      tile.SetData(std::move(typed_data));
      // Return output buffer to pool
      pool.ReturnBuffer(std::move(output_data));
      break;
    }
    default:
      throw std::invalid_argument("Unsupported pixel type");
  }

  // Return scratch buffer to pool for reuse
  pool.ReturnBuffer(std::move(tiff_tile_data));
}

void TiffSource::ReadStripData(Tile& tile, TIFF* handle) const {
  uint32_t rows_per_strip;
  TIFFGetField(handle, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);

  size_t strip_size = TIFFStripSize(handle);
  // Use buffer pool to reuse scratch buffers across tile reads
  auto& pool = GetBufferPool();
  std::vector<uint8_t> strip_data = pool.GetBuffer(strip_size);

  // Allocate buffer for output tile data (in bytes)
  size_t pixel_size = GetPixelTypeSize(dimensions_.pixel_type);
  size_t num_elements = tile.width * tile.height * tile.channels;
  std::vector<uint8_t> output_data = pool.GetBuffer(num_elements * pixel_size);

  // Read overlapping strips
  for (int row = tile.y; row < tile.y + tile.height; row += rows_per_strip) {
    int strip_num = row / rows_per_strip;
    if (TIFFReadEncodedStrip(handle, strip_num, strip_data.data(), strip_size) <
        0) {
      throw std::runtime_error("Failed to read TIFF strip");
    }

    // Copy relevant portion of strip data to output buffer
    CopyStripData(strip_data, output_data, tile, row, rows_per_strip);
  }

  // Set tile data using typed interface
  switch (dimensions_.pixel_type) {
    case PixelType::kUInt8:
      tile.SetData(std::move(output_data));
      break;
    case PixelType::kUInt16: {
      // Reinterpret byte buffer as uint16_t vector
      std::vector<uint16_t> typed_data(num_elements);
      std::memcpy(typed_data.data(), output_data.data(),
                  num_elements * sizeof(uint16_t));
      tile.SetData(std::move(typed_data));
      // Return output buffer to pool
      pool.ReturnBuffer(std::move(output_data));
      break;
    }
    case PixelType::kFloat32: {
      // Reinterpret byte buffer as float vector
      std::vector<float> typed_data(num_elements);
      std::memcpy(typed_data.data(), output_data.data(),
                  num_elements * sizeof(float));
      tile.SetData(std::move(typed_data));
      // Return output buffer to pool
      pool.ReturnBuffer(std::move(output_data));
      break;
    }
    default:
      throw std::invalid_argument("Unsupported pixel type");
  }

  // Return scratch buffer to pool for reuse
  pool.ReturnBuffer(std::move(strip_data));
}

void TiffSource::CopyTileData(const std::vector<uint8_t>& src_data,
                              std::vector<uint8_t>& dst_data, const Tile& tile,
                              int tile_x, int tile_y, int tile_width,
                              int tile_height) const {
  // Calculate intersection of source tile and requested tile
  int src_x_start = std::max(0, tile.x - tile_x);
  int src_y_start = std::max(0, tile.y - tile_y);
  int src_x_end = std::min(tile_width, tile.x + tile.width - tile_x);
  int src_y_end = std::min(tile_height, tile.y + tile.height - tile_y);

  int dst_x_start = std::max(0, tile_x - tile.x);
  int dst_y_start = std::max(0, tile_y - tile.y);

  size_t pixel_size = GetPixelTypeSize(dimensions_.pixel_type);

  // Copy data row by row (accounting for pixel size)
  for (int y = src_y_start; y < src_y_end; ++y) {
    size_t src_row_offset = y * tile_width * tile.channels * pixel_size;
    size_t dst_row_offset = (dst_y_start + y - src_y_start) * tile.width *
                            tile.channels * pixel_size;

    size_t copy_bytes = (src_x_end - src_x_start) * tile.channels * pixel_size;
    std::memcpy(dst_data.data() + dst_row_offset +
                    dst_x_start * tile.channels * pixel_size,
                src_data.data() + src_row_offset +
                    src_x_start * tile.channels * pixel_size,
                copy_bytes);
  }
}

void TiffSource::CopyStripData(const std::vector<uint8_t>& src_data,
                               std::vector<uint8_t>& dst_data, const Tile& tile,
                               int strip_row, int rows_per_strip) const {
  // Calculate intersection of source strip and requested tile
  int src_y_start = std::max(0, tile.y - strip_row);
  int src_y_end = std::min(rows_per_strip, tile.y + tile.height - strip_row);

  int dst_y_start = std::max(0, strip_row - tile.y);

  size_t pixel_size = GetPixelTypeSize(dimensions_.pixel_type);

  // Copy data row by row (accounting for pixel size)
  for (int y = src_y_start; y < src_y_end; ++y) {
    size_t src_row_offset =
        y * dimensions_.GetWidth() * tile.channels * pixel_size;
    size_t dst_row_offset = (dst_y_start + y - src_y_start) * tile.width *
                            tile.channels * pixel_size;

    std::memcpy(
        dst_data.data() + dst_row_offset,
        src_data.data() + src_row_offset + tile.x * tile.channels * pixel_size,
        tile.width * tile.channels * pixel_size);
  }
}

void TiffSource::InitializeHandlePool() {
  // Create a pool of read-only TIFF handles for parallel access
  // Use 4 handles as a reasonable default for most systems
  const int kPoolSize = 4;
  std::lock_guard<std::mutex> lock(pool_mutex_);
  handle_pool_.reserve(kPoolSize);
  for (int i = 0; i < kPoolSize; ++i) {
    TIFF* handle = OpenTiffForRead(filename_, "r");
    if (!handle) {
      continue;
    }
    if (!TIFFSetDirectory(handle, static_cast<tdir_t>(directory_index_))) {
      TIFFClose(handle);
      continue;
    }
    handle_pool_.push_back(handle);
  }
}

TIFF* TiffSource::AcquireHandle() const {
  std::lock_guard<std::mutex> lock(pool_mutex_);
  if (!handle_pool_.empty()) {
    TIFF* handle = handle_pool_.back();
    handle_pool_.pop_back();
    return handle;
  }
  // Pool is empty, create a new temporary handle
  TIFF* handle = OpenTiffForRead(filename_, "r");
  if (!handle) {
    throw std::runtime_error("Failed to open TIFF handle for parallel read: " +
                             filename_.string());
  }
  if (!TIFFSetDirectory(handle, static_cast<tdir_t>(directory_index_))) {
    TIFFClose(handle);
    throw std::runtime_error("Failed to set TIFF directory index: " +
                             std::to_string(directory_index_));
  }
  return handle;
}

void TiffSource::ReleaseHandle(TIFF* handle) const {
  if (!handle) {
    return;
  }
  std::lock_guard<std::mutex> lock(pool_mutex_);
  handle_pool_.push_back(handle);
}

}  // namespace fim

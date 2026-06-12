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
 * @file fimage_source.cpp
 * @brief Implementation of FImage native format source.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the implementation of the FImageSource class for reading
 * FImage files with support for tiling, compression, and efficient random
 * access.
 */
#include "fim/sources/fimage_source.h"

#include <algorithm>
#include <stdexcept>

#include "fim/utilities/buffer_pool.h"
#include "fim/utilities/decompressor.h"

namespace fim {

FImageSource FImageSource::Create(const fs::path& filename) {
  if (!fs::exists(filename)) {
    throw std::runtime_error("FImage file does not exist: " +
                             filename.string());
  }
  return FImageSource(filename);
}

FImageSource::FImageSource(const fs::path& filename)
    : SourceBase<FImageSource>(),
      filename_(filename),
      file_(std::make_unique<std::ifstream>(filename, std::ios::binary)),
      is_tiled_(false) {
  if (!file_->is_open()) {
    throw std::runtime_error("Failed to open FImage file: " +
                             filename_.string());
  }

  ReadHeader();

  // Determine if tiled (need this before loading seek table)
  is_tiled_ = (header_.tile_width > 0 && header_.tile_height > 0);

  if (header_.compression !=
      static_cast<std::uint8_t>(CompressionType::kNone)) {
    LoadSeekTable();
  }

  // Set up dimensions
  dimensions_ = ImageInfo(header_.width, header_.height, header_.channels,
                          static_cast<PixelType>(header_.pixel_type),
                          static_cast<DataLayout>(header_.data_layout));
}

FImageSource::~FImageSource() {
  if (file_ && file_->is_open()) {
    file_->close();
  }
}

FImageSource::FImageSource(FImageSource&& other) noexcept
    : SourceBase<FImageSource>(),
      filename_(std::move(other.filename_)),
      file_(std::move(other.file_)),
      header_(std::move(other.header_)),
      seek_table_(std::move(other.seek_table_)),
      dimensions_(std::move(other.dimensions_)),
      is_tiled_(other.is_tiled_) {}

FImageSource& FImageSource::operator=(FImageSource&& other) noexcept {
  if (this != &other) {
    filename_ = std::move(other.filename_);
    file_ = std::move(other.file_);
    header_ = std::move(other.header_);
    seek_table_ = std::move(other.seek_table_);
    dimensions_ = std::move(other.dimensions_);
    is_tiled_ = other.is_tiled_;
  }
  return *this;
}

void FImageSource::ReadHeader() {
  std::lock_guard<std::mutex> lock(file_mutex_);

  file_->seekg(0);
  if (!header_.ReadFrom(*file_)) {
    throw std::runtime_error("Failed to read FImage header from: " +
                             filename_.string());
  }

  if (!header_.IsValid()) {
    throw std::runtime_error("Invalid FImage header (bad magic/version): " +
                             filename_.string());
  }
}

void FImageSource::LoadSeekTable() {
  std::lock_guard<std::mutex> lock(file_mutex_);

  if (header_.seek_table_offset == 0) {
    throw std::runtime_error("Compressed file missing seek table offset: " +
                             filename_.string());
  }

  // Determine number of entries
  size_t num_entries;
  if (is_tiled_) {
    num_entries = header_.num_tiles_x * header_.num_tiles_y;
  } else {
    num_entries = 1;  // Contiguous mode has single entry
  }

  // Seek to table
  file_->seekg(header_.seek_table_offset);

  // Read all entries
  seek_table_.resize(num_entries);
  for (size_t i = 0; i < num_entries; ++i) {
    if (!seek_table_[i].ReadFrom(*file_)) {
      throw std::runtime_error("Failed to read seek table entry " +
                               std::to_string(i) +
                               " from: " + filename_.string());
    }
  }
}

std::vector<uint8_t> FImageSource::ReadTileData(int tile_x, int tile_y) const {
  int tile_index = tile_y * header_.num_tiles_x + tile_x;
  size_t tile_size = static_cast<size_t>(header_.tile_width) *
                     static_cast<size_t>(header_.tile_height) *
                     static_cast<size_t>(header_.channels) *
                     header_.GetPixelSize();

  auto compression = static_cast<CompressionType>(header_.compression);
  auto& pool = GetBufferPool();

  if (compression == CompressionType::kNone) {
    // Uncompressed: direct offset calculation
    std::uint64_t offset =
        256 + static_cast<std::uint64_t>(tile_index) * tile_size;

    // Critical section: Only hold lock during file I/O
    std::vector<uint8_t> tile_data = pool.GetBuffer(tile_size);
    {
      std::lock_guard<std::mutex> lock(file_mutex_);
      file_->seekg(offset);
      file_->read(reinterpret_cast<char*>(tile_data.data()), tile_size);

      if (!file_->good()) {
        throw std::runtime_error("Failed to read tile data");
      }
    }
    // Lock released - uncompressed data ready to return

    return tile_data;
  } else {
    // Compressed: use seek table
    if (static_cast<size_t>(tile_index) >= seek_table_.size()) {
      throw std::runtime_error("Tile index out of bounds: " +
                               std::to_string(tile_index));
    }

    const auto& entry = seek_table_[tile_index];

    // Critical section: Only hold lock during file I/O
    std::vector<uint8_t> compressed_data =
        pool.GetBuffer(entry.compressed_size);
    {
      std::lock_guard<std::mutex> lock(file_mutex_);
      file_->seekg(entry.offset);
      file_->read(reinterpret_cast<char*>(compressed_data.data()),
                  entry.compressed_size);

      if (!file_->good()) {
        throw std::runtime_error("Failed to read compressed tile data");
      }
    }
    // Lock released - now decompress outside critical section

    // Decompression happens OUTSIDE the lock, allowing parallel processing
    auto decompress_result =
        DecompressData(compressed_data, tile_size, compression);

    // Return compressed buffer to pool
    pool.ReturnBuffer(std::move(compressed_data));

    if (!decompress_result.ok()) {
      throw std::runtime_error(
          "Decompression failed: " +
          std::string(decompress_result.status().message()));
    }
    return std::move(*decompress_result);
  }
}

std::vector<uint8_t> FImageSource::ReadContiguousRegion(int x, int y, int width,
                                                        int height) const {
  auto compression = static_cast<CompressionType>(header_.compression);
  const DataLayout layout = static_cast<DataLayout>(header_.data_layout);
  const size_t pixel_size = header_.GetPixelSize();

  if (compression == CompressionType::kNone) {
    // Uncompressed: read directly from file
    std::vector<uint8_t> region_data(width * height * header_.channels *
                                     pixel_size);

    // Critical section: Only hold lock during file I/O
    {
      std::lock_guard<std::mutex> lock(file_mutex_);

      if (layout == DataLayout::kChannelsLast) {
        for (int row = 0; row < height; ++row) {
          std::uint64_t row_offset =
              256 + static_cast<std::uint64_t>(((y + row) * header_.width + x) *
                                               header_.channels * pixel_size);
          file_->seekg(row_offset);

          size_t row_bytes = width * header_.channels * pixel_size;
          file_->read(
              reinterpret_cast<char*>(region_data.data() + row * row_bytes),
              row_bytes);

          if (!file_->good()) {
            throw std::runtime_error("Failed to read contiguous region");
          }
        }
      } else {
        // Channels-first (CHW): read per-plane rows.
        for (uint32_t c = 0; c < header_.channels; ++c) {
          for (int row = 0; row < height; ++row) {
            const std::uint64_t src_offset =
                256 + static_cast<std::uint64_t>(
                          ((static_cast<std::uint64_t>(c) * header_.height +
                            static_cast<std::uint64_t>(y + row)) *
                               header_.width +
                           static_cast<std::uint64_t>(x)) *
                          pixel_size);
            file_->seekg(src_offset);

            const size_t row_bytes = static_cast<size_t>(width) * pixel_size;
            const size_t dst_offset =
                (static_cast<size_t>(c) * static_cast<size_t>(height) +
                 static_cast<size_t>(row)) *
                static_cast<size_t>(width) * pixel_size;
            file_->read(
                reinterpret_cast<char*>(region_data.data() + dst_offset),
                row_bytes);

            if (!file_->good()) {
              throw std::runtime_error("Failed to read contiguous region");
            }
          }
        }
      }
    }
    // Lock released

    return region_data;
  } else {
    // Compressed: decompress entire image, then extract region
    if (seek_table_.empty()) {
      throw std::runtime_error("Seek table not loaded for compressed file");
    }

    const auto& entry = seek_table_[0];

    // Critical section: Only hold lock during file I/O
    std::vector<uint8_t> compressed_data(entry.compressed_size);
    {
      std::lock_guard<std::mutex> lock(file_mutex_);
      file_->seekg(entry.offset);
      file_->read(reinterpret_cast<char*>(compressed_data.data()),
                  entry.compressed_size);

      if (!file_->good()) {
        throw std::runtime_error("Failed to read compressed image data");
      }
    }
    // Lock released - now decompress outside critical section

    size_t full_size = static_cast<size_t>(header_.width) *
                       static_cast<size_t>(header_.height) *
                       static_cast<size_t>(header_.channels) *
                       header_.GetPixelSize();

    auto decompress_result =
        DecompressData(compressed_data, full_size, compression);
    if (!decompress_result.ok()) {
      throw std::runtime_error(
          "Decompression failed: " +
          std::string(decompress_result.status().message()));
    }
    std::vector<uint8_t> full_data = std::move(*decompress_result);

    return ExtractRegion(full_data, x, y, width, height);
  }
}

std::vector<uint8_t> FImageSource::ExtractRegion(
    const std::vector<uint8_t>& full_data, int x, int y, int width,
    int height) const {
  const DataLayout layout = static_cast<DataLayout>(header_.data_layout);
  const size_t pixel_size = header_.GetPixelSize();
  std::vector<uint8_t> region_data(width * height * header_.channels *
                                   pixel_size);

  if (layout == DataLayout::kChannelsLast) {
    for (int row = 0; row < height; ++row) {
      size_t src_offset =
          ((y + row) * header_.width + x) * header_.channels * pixel_size;
      size_t dst_offset = row * width * header_.channels * pixel_size;
      std::copy_n(&full_data[src_offset], width * header_.channels * pixel_size,
                  &region_data[dst_offset]);
    }
  } else {
    // Channels-first (CHW)
    for (uint32_t c = 0; c < header_.channels; ++c) {
      for (int row = 0; row < height; ++row) {
        const size_t src_offset =
            ((static_cast<size_t>(c) * static_cast<size_t>(header_.height) +
              static_cast<size_t>(y + row)) *
                 static_cast<size_t>(header_.width) +
             static_cast<size_t>(x)) *
            pixel_size;
        const size_t dst_offset =
            ((static_cast<size_t>(c) * static_cast<size_t>(height) +
              static_cast<size_t>(row)) *
             static_cast<size_t>(width)) *
            pixel_size;
        std::copy_n(&full_data[src_offset],
                    static_cast<size_t>(width) * pixel_size,
                    &region_data[dst_offset]);
      }
    }
  }

  return region_data;
}

ImageInfo FImageSource::GetDimensions() const {
  return dimensions_;
}

int FImageSource::GetChannels() const {
  return dimensions_.channels;
}

DataLayout FImageSource::GetMemoryLayout() const {
  return dimensions_.layout;
}

TileSize FImageSource::GetIdealTileSize() const {
  if (is_tiled_) {
    return TileSize(header_.tile_width, header_.tile_height);
  }
  // For contiguous files, suggest a reasonable default
  return TileSize(256, 256);
}

Tile FImageSource::GetTile(int x, int y, int width, int height) const {
  // Validate bounds
  if (x < 0 || y < 0 || width <= 0 || height <= 0) {
    throw std::runtime_error("Invalid tile request parameters");
  }

  if (x + width > dimensions_.GetWidth() ||
      y + height > dimensions_.GetHeight()) {
    throw std::runtime_error("Tile request out of bounds");
  }

  if (is_tiled_) {
    // Tiled mode: may need to read multiple tiles
    int start_tile_x = x / header_.tile_width;
    int start_tile_y = y / header_.tile_height;
    int end_tile_x = (x + width - 1) / header_.tile_width;
    int end_tile_y = (y + height - 1) / header_.tile_height;

    size_t pixel_size = header_.GetPixelSize();
    const DataLayout layout = static_cast<DataLayout>(header_.data_layout);
    // Allocate result buffer
    std::vector<uint8_t> result(width * height * header_.channels * pixel_size);

    // Read and composite tiles
    for (int tile_y = start_tile_y; tile_y <= end_tile_y; ++tile_y) {
      for (int tile_x = start_tile_x; tile_x <= end_tile_x; ++tile_x) {
        std::vector<uint8_t> tile_data = ReadTileData(tile_x, tile_y);

        // Calculate overlap region
        int tile_start_x = tile_x * header_.tile_width;
        int tile_start_y = tile_y * header_.tile_height;

        int overlap_x = std::max(x, tile_start_x);
        int overlap_y = std::max(y, tile_start_y);
        int overlap_end_x = std::min(
            x + width, tile_start_x + static_cast<int>(header_.tile_width));
        int overlap_end_y = std::min(
            y + height, tile_start_y + static_cast<int>(header_.tile_height));

        if (layout == DataLayout::kChannelsLast) {
          // Copy overlap region (accounting for pixel size) - HWC
          for (int row = overlap_y; row < overlap_end_y; ++row) {
            int tile_row = row - tile_start_y;
            int result_row = row - y;

            for (int col = overlap_x; col < overlap_end_x; ++col) {
              int tile_col = col - tile_start_x;
              int result_col = col - x;

              size_t src_idx = (tile_row * header_.tile_width + tile_col) *
                               header_.channels * pixel_size;
              size_t dst_idx = (result_row * width + result_col) *
                               header_.channels * pixel_size;

              std::copy_n(&tile_data[src_idx], header_.channels * pixel_size,
                          &result[dst_idx]);
            }
          }
        } else {
          // Channels-first (CHW): copy per plane row segments for the overlap.
          const int overlap_w = overlap_end_x - overlap_x;
          for (int row = overlap_y; row < overlap_end_y; ++row) {
            int tile_row = row - tile_start_y;
            int result_row = row - y;
            const int tile_col0 = overlap_x - tile_start_x;
            const int result_col0 = overlap_x - x;
            for (uint32_t c = 0; c < header_.channels; ++c) {
              const size_t src_idx =
                  ((static_cast<size_t>(c) *
                        static_cast<size_t>(header_.tile_height) +
                    static_cast<size_t>(tile_row)) *
                       static_cast<size_t>(header_.tile_width) +
                   static_cast<size_t>(tile_col0)) *
                  pixel_size;
              const size_t dst_idx =
                  ((static_cast<size_t>(c) * static_cast<size_t>(height) +
                    static_cast<size_t>(result_row)) *
                       static_cast<size_t>(width) +
                   static_cast<size_t>(result_col0)) *
                  pixel_size;
              const size_t bytes = static_cast<size_t>(overlap_w) * pixel_size;
              std::copy_n(&tile_data[src_idx], bytes, &result[dst_idx]);
            }
          }
        }
      }
    }

    Tile tile(x, y, width, height, header_.channels,
              static_cast<DataLayout>(header_.data_layout),
              static_cast<PixelType>(header_.pixel_type));
    // Set tile data using typed interface
    switch (static_cast<PixelType>(header_.pixel_type)) {
      case PixelType::kUInt8:
        tile.SetData(std::move(result));
        break;
      case PixelType::kUInt16: {
        size_t num_elements = width * height * header_.channels;
        std::vector<uint16_t> typed_data(num_elements);
        std::memcpy(typed_data.data(), result.data(),
                    num_elements * sizeof(uint16_t));
        tile.SetData(std::move(typed_data));
        break;
      }
      case PixelType::kFloat32: {
        size_t num_elements = width * height * header_.channels;
        std::vector<float> typed_data(num_elements);
        std::memcpy(typed_data.data(), result.data(),
                    num_elements * sizeof(float));
        tile.SetData(std::move(typed_data));
        break;
      }
      default:
        throw std::invalid_argument("Unsupported pixel type");
    }
    return tile;
  } else {
    // Contiguous mode
    std::vector<uint8_t> region_data =
        ReadContiguousRegion(x, y, width, height);

    Tile tile(x, y, width, height, header_.channels,
              static_cast<DataLayout>(header_.data_layout),
              static_cast<PixelType>(header_.pixel_type));
    // Set tile data using typed interface
    switch (static_cast<PixelType>(header_.pixel_type)) {
      case PixelType::kUInt8:
        tile.SetData(std::move(region_data));
        break;
      case PixelType::kUInt16: {
        size_t num_elements = static_cast<size_t>(width) *
                              static_cast<size_t>(height) *
                              static_cast<size_t>(header_.channels);
        size_t required_bytes = num_elements * sizeof(uint16_t);
        if (region_data.size() < required_bytes) {
          throw std::runtime_error(
              "Insufficient data for kUInt16 tile: need " +
              std::to_string(required_bytes) + " bytes, got " +
              std::to_string(region_data.size()) + " bytes");
        }
        std::vector<uint16_t> typed_data(num_elements);
        std::memcpy(typed_data.data(), region_data.data(), required_bytes);
        tile.SetData(std::move(typed_data));
        break;
      }
      case PixelType::kFloat32: {
        size_t num_elements = static_cast<size_t>(width) *
                              static_cast<size_t>(height) *
                              static_cast<size_t>(header_.channels);
        size_t required_bytes = num_elements * sizeof(float);
        if (region_data.size() < required_bytes) {
          throw std::runtime_error(
              "Insufficient data for kFloat32 tile: need " +
              std::to_string(required_bytes) + " bytes, got " +
              std::to_string(region_data.size()) + " bytes");
        }
        std::vector<float> typed_data(num_elements);
        std::memcpy(typed_data.data(), region_data.data(), required_bytes);
        tile.SetData(std::move(typed_data));
        break;
      }
      default:
        throw std::invalid_argument("Unsupported pixel type");
    }
    return tile;
  }
}

}  // namespace fim

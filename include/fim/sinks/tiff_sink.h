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
 * @file tiff_sink.h
 * @brief TIFF image sink implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the TiffSink class which provides TIFF image writing
 * capabilities for the fim image processing pipeline. It uses libtiff for
 * TIFF encoding and supports both tiled and strip-based TIFF output with
 * configurable compression and tile sizes. It also supports pyramidal TIFF
 * generation for efficient multi-resolution image storage.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SINKS_TIFF_SINK_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SINKS_TIFF_SINK_H_

#include <tiffio.h>

#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "aifocore/platform/portability.h"
#include "fim/operators/downsample.h"
#include "fim/pipeline.h"
#include "fim/sources/memory_source.h"
#include "fim/types.h"

namespace fim {

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

inline TIFF* OpenTiffForWrite(const std::filesystem::path& filename,
                              const char* mode) {
  // Open the underlying stream as read/write so libtiff can seek back and
  // read previously written directory entries when writing pyramidal
  // (multi-IFD) TIFFs. With "wb" (write-only), libtiff's TIFFLinkDirectory
  // / TIFFRewriteDirectory fail with "Error fetching directory count".
  FILE* file = aifocore::portable_fopen(filename, "w+b");
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

/**
 * @brief TIFF image sink for writing single-page and pyramidal TIFF files.
 *
 * This class provides a unified sink implementation for TIFF image files using
 * libtiff. It supports both single-page and pyramidal (multi-page) TIFF output
 * and can handle various channel configurations (grayscale, RGB, RGBA).
 *
 * For pyramidal output, it creates multiple pages with progressively smaller
 * resolutions until the smallest dimension is below the tile size. The
 * implementation optimizes memory usage by switching from source-based to
 * memory-based downsampling when image size becomes manageable.
 *
 * The class follows the CRTP pattern by inheriting from SinkBase and provides
 * the required interface methods for image sinks.
 *
 * @note Use Create() factory method to instantiate.
 */
class TiffSink : public SinkBase<TiffSink> {
 public:
  /**
   * @brief Creates a TIFF sink with default settings (factory method).
   *
   * Uses default tile size of 256x256 pixels for tiled output.
   * Creates a single-page TIFF file.
   *
   * @param filename Path to the output TIFF file where the image will be
   * written
   * @return TiffSink instance
   *
   * @note Future: This will be migrated to return absl::StatusOr<TiffSink>
   */
  static TiffSink Create(const fs::path& filename);

  /**
   * @brief Creates a TIFF sink with custom tile size (factory method).
   *
   * Creates a single-page TIFF file with the specified tile size.
   *
   * @param filename Path to the output TIFF file where the image will be
   * written
   * @param tile_size Preferred tile size for tiled output
   * @return TiffSink instance
   *
   * @note Future: This will be migrated to return absl::StatusOr<TiffSink>
   */
  static TiffSink Create(const fs::path& filename, const TileSize& tile_size);

  /**
   * @brief Creates a TIFF sink with full configuration (factory method).
   *
   * @param filename Path to the output TIFF file where the image will be
   * written
   * @param tile_size Preferred tile size for all pyramid levels
   * @param pyramidal Whether to create a pyramidal (multi-page) TIFF
   * @param memory_threshold_mb Memory threshold in MB for switching to
   * memory-based downsampling
   * @param downsample_factor Factor by which to downsample each pyramid level
   * @return TiffSink instance
   *
   * @note Future: This will be migrated to return absl::StatusOr<TiffSink>
   */
  static TiffSink Create(const fs::path& filename, const TileSize& tile_size,
                         bool pyramidal, size_t memory_threshold_mb = 50,
                         int downsample_factor = 2);

  /**
   * @brief Default destructor.
   */
  ~TiffSink() = default;

  /**
   * @brief Renders the input to a TIFF file.
   *
   * This method processes the input and creates either a single-page or
   * pyramidal TIFF file based on the pyramidal setting. For pyramidal output,
   * it implements the optimized pyramid generation strategy that switches
   * between source-based and memory-based downsampling.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to render
   * @throw std::runtime_error if the TIFF file cannot be created or written
   */
  template <typename InputType>
  void Render(const InputType& input);

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param filename Path to the output TIFF file
   */
  explicit TiffSink(const fs::path& filename);

  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param filename Path to the output TIFF file
   * @param tile_size Preferred tile size for tiled output
   */
  explicit TiffSink(const fs::path& filename, const TileSize& tile_size);

  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param filename Path to the output TIFF file
   * @param tile_size Preferred tile size for all pyramid levels
   * @param pyramidal Whether to create a pyramidal (multi-page) TIFF
   * @param memory_threshold_mb Memory threshold in MB
   * @param downsample_factor Factor by which to downsample each pyramid level
   */
  explicit TiffSink(const fs::path& filename, const TileSize& tile_size,
                    bool pyramidal, size_t memory_threshold_mb,
                    int downsample_factor);

  /**
   * @brief Structure to hold tile data for queued TIFF writing.
   */
  struct TiffTileWrite {
    std::vector<uint8_t> data;  ///< Tile pixel data
    uint32_t x;                 ///< X coordinate of the tile
    uint32_t y;                 ///< Y coordinate of the tile
  };

  /**
   * @brief Renders a single-page TIFF file.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to render
   */
  template <typename InputType>
  void RenderSinglePage(const InputType& input);

  /**
   * @brief Renders a pyramidal TIFF file.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to render
   */
  template <typename InputType>
  void RenderPyramidal(const InputType& input);

  /**
   * @brief Writes a single level of the pyramid to the TIFF file.
   *
   * @tparam InputType The type of the input source or operator
   * @param tiff Pointer to the TIFF file handle
   * @param input The input source or operator for this level
   * @param level_index The pyramid level index (0 = full resolution)
   */
  template <typename InputType>
  void WriteLevel(TIFF* tiff, const InputType& input, int level_index = 0);

  /**
   * @brief Writes the image as a tiled TIFF level (serial version).
   *
   * @tparam InputType The type of the input source or operator
   * @param tiff Pointer to the TIFF file handle
   * @param input The input source or operator to write
   */
  template <typename InputType>
  void WriteTiledLevelSerial(TIFF* tiff, const InputType& input);

  /**
   * @brief Writes the image as a tiled TIFF level (parallel version).
   *
   * Uses a producer-consumer pattern with parallel tile generation
   * and a dedicated writer thread for TIFF I/O.
   *
   * @tparam InputType The type of the input source or operator
   * @param tiff Pointer to the TIFF file handle
   * @param input The input source or operator to write
   */
  template <typename InputType>
  void WriteTiledLevelParallel(TIFF* tiff, const InputType& input);

  /**
   * @brief Writes the image as a strip-based TIFF level.
   *
   * @tparam InputType The type of the input source or operator
   * @param tiff Pointer to the TIFF file handle
   * @param input The input source or operator to write
   */
  template <typename InputType>
  void WriteStripLevel(TIFF* tiff, const InputType& input);

  /**
   * @brief Configures TIFF tags for a level.
   *
   * @param tiff Pointer to the TIFF file handle
   * @param dims Image dimensions for this level
   * @param level_index The pyramid level index (0 = full resolution)
   */
  void ConfigureTiffTags(TIFF* tiff, const ImageInfo& dims,
                         int level_index = 0);

  /**
   * @brief Assembles a complete image level into memory.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to assemble
   * @return TileData variant containing the assembled image data in the
   * appropriate type
   */
  template <typename InputType>
  TileData AssembleImageData(const InputType& input);

  /**
   * @brief Assembles a downsampled image level into memory.
   *
   * This version works with const references to the input, making it suitable
   * for cases where we can't take ownership of the input (e.g., when input is
   * a const reference parameter). It performs type-safe downsampling by
   * fetching and averaging regions from the input.
   *
   * @tparam InputType The type of the input source or operator
   * @param input The input source or operator to downsample and assemble
   * @param downsample_factor The factor by which to downsample
   * @param target_dims The target dimensions after downsampling
   * @return TileData variant containing the assembled downsampled image data in
   * the appropriate type
   */
  template <typename InputType>
  TileData AssembleDownsampledData(const InputType& input,
                                   int downsample_factor,
                                   const ImageInfo& target_dims);

  /**
   * @brief Calculates the uncompressed memory size of an image level.
   *
   * @param dims Image dimensions
   * @return Memory size in bytes
   */
  static size_t CalculateMemorySize(const ImageInfo& dims);

  /**
   * @brief Checks if downsampling should continue for the given dimensions.
   *
   * @param dims Current level dimensions
   * @return true if downsampling should continue, false otherwise
   */
  bool ShouldContinueDownsampling(const ImageInfo& dims) const;

  TileSize tile_size_;             ///< Tile size for all levels
  bool pyramidal_;                 ///< Whether to create pyramidal TIFF
  size_t memory_threshold_bytes_;  ///< Memory threshold for switching strategy
                                   ///< (pyramidal only)
  int downsample_factor_;  ///< Downsample factor between levels (pyramidal
                           ///< only)
};

template <typename InputType>
void TiffSink::Render(const InputType& input) {
  auto dims = input.GetDimensions();
  if (dims.layout != DataLayout::kChannelsLast) {
    throw std::runtime_error(
        "TIFF output currently only supports DataLayout::kChannelsLast "
        "(interleaved/HWC). Use .ToLayout(kChannelsLast) / Image.to_layout().");
  }
  if (pyramidal_) {
    RenderPyramidal(input);
  } else {
    RenderSinglePage(input);
  }
}

template <typename InputType>
void TiffSink::RenderSinglePage(const InputType& input) {
  TIFF* tiff = OpenTiffForWrite(filename_, "w8");
  if (!tiff) {
    throw std::runtime_error("Failed to create TIFF file: " +
                             filename_.string());
  }

  try {
    WriteLevel(tiff, input, 0);
    TIFFClose(tiff);
  } catch (...) {
    TIFFClose(tiff);
    throw;
  }
}

template <typename InputType>
void TiffSink::RenderPyramidal(const InputType& input) {
  TIFF* tiff = OpenTiffForWrite(filename_, "w8");
  if (!tiff) {
    throw std::runtime_error("Failed to create TIFF file: " +
                             filename_.string());
  }

  try {
    int level = 0;
    std::unique_ptr<MemorySource> memory_source;

    // Write level 0 (original resolution)
    auto current_dims = input.GetDimensions();
    WriteLevel(tiff, input, level);

    // Check if level 0 should be kept in memory
    if (CalculateMemorySize(current_dims) <= memory_threshold_bytes_) {
      auto image_data = AssembleImageData(input);
      memory_source = std::make_unique<MemorySource>(MemorySource::Create(
          std::move(image_data), current_dims, tile_size_));
    }

    level++;

    while (ShouldContinueDownsampling(current_dims)) {
      // Create next directory for this level
      if (TIFFWriteDirectory(tiff) != 1) {
        throw std::runtime_error("Failed to write TIFF directory");
      }

      if (memory_source) {
        // Downsample from the current memory source
        // We need to move the source, so we'll recreate it if needed
        auto downsampled_input =
            Downsample(std::move(*memory_source), downsample_factor_);
        current_dims = downsampled_input.GetDimensions();
        WriteLevel(tiff, downsampled_input, level);

        // Always recreate the memory source for this level if it fits in memory
        if (CalculateMemorySize(current_dims) <= memory_threshold_bytes_) {
          auto image_data = AssembleImageData(downsampled_input);
          memory_source = std::make_unique<MemorySource>(MemorySource::Create(
              std::move(image_data), current_dims, tile_size_));
        } else {
          // This level is too large, clear the memory source
          memory_source.reset();
        }
      } else {
        // No memory source available - materialize the previous level
        // This happens when previous level was too large to keep in memory
        // We need to go back to the original input and downsample from there

        // For now, we'll fall back to rendering from the input with cumulative
        // downsample This is less efficient but works with type-erased inputs
        int cumulative_factor = 1;
        for (int i = 0; i < level; ++i) {
          cumulative_factor *= downsample_factor_;
        }

        // Calculate target dimensions for downsampled level
        current_dims = ImageInfo(
            (input.GetDimensions().GetWidth() + cumulative_factor - 1) /
                cumulative_factor,
            (input.GetDimensions().GetHeight() + cumulative_factor - 1) /
                cumulative_factor,
            input.GetDimensions().channels, input.GetDimensions().pixel_type,
            input.GetDimensions().layout);

        // Assemble the downsampled data using type-safe block-based approach
        TileData level_data =
            AssembleDownsampledData(input, cumulative_factor, current_dims);

        MemorySource temp_source = MemorySource::Create(
            std::move(level_data), current_dims, tile_size_);
        WriteLevel(tiff, temp_source, level);

        // Check if we should start keeping levels in memory
        if (CalculateMemorySize(current_dims) <= memory_threshold_bytes_) {
          // Reassemble for next iteration
          auto image_data =
              AssembleDownsampledData(input, cumulative_factor, current_dims);
          memory_source = std::make_unique<MemorySource>(MemorySource::Create(
              std::move(image_data), current_dims, tile_size_));
        }
      }

      level++;
    }

    TIFFClose(tiff);
  } catch (...) {
    TIFFClose(tiff);
    throw;
  }
}

template <typename InputType>
void TiffSink::WriteLevel(TIFF* tiff, const InputType& input, int level_index) {
  auto dims = input.GetDimensions();
  ConfigureTiffTags(tiff, dims, level_index);

  // Choose between tiled and strip based on dimensions and tile size
  if (dims.GetWidth() > tile_size_.width ||
      dims.GetHeight() > tile_size_.height) {
    // Calculate number of tiles
    size_t num_tiles_x =
        (dims.GetWidth() + tile_size_.width - 1) / tile_size_.width;
    size_t num_tiles_y =
        (dims.GetHeight() + tile_size_.height - 1) / tile_size_.height;
    size_t num_tiles = num_tiles_x * num_tiles_y;

    // Decide between parallel and serial based on workload
    if (ShouldParallelize(num_tiles)) {
      WriteTiledLevelParallel(tiff, input);
    } else {
      WriteTiledLevelSerial(tiff, input);
    }
  } else {
    WriteStripLevel(tiff, input);
  }
}

template <typename InputType>
void TiffSink::WriteTiledLevelSerial(TIFF* tiff, const InputType& input) {
  auto dims = input.GetDimensions();

  TIFFSetField(tiff, TIFFTAG_TILEWIDTH, tile_size_.width);
  TIFFSetField(tiff, TIFFTAG_TILELENGTH, tile_size_.height);

  // Write tiles
  for (int y = 0; y < dims.GetHeight(); y += tile_size_.height) {
    for (int x = 0; x < dims.GetWidth(); x += tile_size_.width) {
      int actual_width = std::min(tile_size_.width, dims.GetWidth() - x);
      int actual_height = std::min(tile_size_.height, dims.GetHeight() - y);

      Tile tile = input.GetTile(x, y, actual_width, actual_height);

      // Set padding information for border tiles
      tile.expected_width = tile_size_.width;
      tile.expected_height = tile_size_.height;
      tile.needs_padding =
          (tile.width < tile_size_.width || tile.height < tile_size_.height);

      std::vector<uint8_t> tiff_data = tile.CreatePaddedDataAsBytes();
      if (TIFFWriteTile(tiff, tiff_data.data(), static_cast<uint32_t>(x),
                        static_cast<uint32_t>(y), 0, 0) < 0) {
        throw std::runtime_error("Failed to write TIFF tile");
      }
    }
  }
}

template <typename InputType>
void TiffSink::WriteTiledLevelParallel(TIFF* tiff, const InputType& input) {
  auto dims = input.GetDimensions();

  TIFFSetField(tiff, TIFFTAG_TILEWIDTH, tile_size_.width);
  TIFFSetField(tiff, TIFFTAG_TILELENGTH, tile_size_.height);

  // Build list of tile coordinates
  std::vector<std::pair<int, int>> tile_coords;
  for (int y = 0; y < dims.GetHeight(); y += tile_size_.height) {
    for (int x = 0; x < dims.GetWidth(); x += tile_size_.width) {
      tile_coords.push_back({x, y});
    }
  }

  // Thread-safe queue for write operations
  std::queue<TiffTileWrite> write_queue;
  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  bool done_producing = false;
  std::exception_ptr writer_exception = nullptr;

  // Launch dedicated writer thread
  std::thread writer_thread([&]() {
    try {
      while (true) {
        TiffTileWrite tile_write;
        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          queue_cv.wait(lock,
                        [&] { return !write_queue.empty() || done_producing; });

          if (write_queue.empty() && done_producing) {
            break;
          }

          if (!write_queue.empty()) {
            tile_write = std::move(write_queue.front());
            write_queue.pop();
          } else {
            continue;
          }
        }

        // Write to TIFF (not thread-safe, must be sequential)
        if (TIFFWriteTile(tiff, tile_write.data.data(), tile_write.x,
                          tile_write.y, 0, 0) < 0) {
          throw std::runtime_error("Failed to write TIFF tile");
        }
      }
    } catch (...) {
      writer_exception = std::current_exception();
    }
  });

  // Process tiles in parallel using thread pool
  auto& pool = GetThreadPool();
  // Capture necessary values to avoid dangling references
  const int tile_width = tile_size_.width;
  const int tile_height = tile_size_.height;
  const int img_width = dims.GetWidth();
  const int img_height = dims.GetHeight();

  try {
    auto futures = pool.submit_blocks(
        0, tile_coords.size(),
        [&input, &tile_coords, tile_width, tile_height, img_width, img_height](
            size_t start, size_t end) {
          // Each worker thread processes a batch of tiles
          std::vector<TiffTileWrite> local_tiles;
          local_tiles.reserve(end - start);

          for (size_t i = start; i < end; ++i) {
            const auto& coord = tile_coords[i];
            int x = coord.first;
            int y = coord.second;
            int actual_width = std::min(tile_width, img_width - x);
            int actual_height = std::min(tile_height, img_height - y);

            // Execute full pipeline for this tile
            Tile tile = input.GetTile(x, y, actual_width, actual_height);

            // Set padding information for border tiles
            tile.expected_width = tile_width;
            tile.expected_height = tile_height;
            tile.needs_padding =
                (tile.width < tile_width || tile.height < tile_height);

            // Prepare tile data for writing
            TiffTileWrite tile_write;
            tile_write.data = tile.CreatePaddedDataAsBytes();
            tile_write.x = static_cast<uint32_t>(x);
            tile_write.y = static_cast<uint32_t>(y);
            local_tiles.push_back(std::move(tile_write));
          }

          return local_tiles;
        });

    // Collect results and push to write queue
    for (auto& future : futures) {
      auto batch = future.get();
      {
        std::lock_guard<std::mutex> lock(queue_mutex);
        for (auto& tile_write : batch) {
          write_queue.push(std::move(tile_write));
        }
      }
      queue_cv.notify_one();
    }

    // Signal writer thread that we're done producing
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      done_producing = true;
    }
    queue_cv.notify_one();

    // Wait for writer thread to finish
    writer_thread.join();

    // Check if writer thread threw an exception
    if (writer_exception) {
      std::rethrow_exception(writer_exception);
    }
  } catch (...) {
    // If an error occurs, signal writer thread and wait for it
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      done_producing = true;
    }
    queue_cv.notify_one();
    writer_thread.join();
    throw;
  }
}

template <typename InputType>
void TiffSink::WriteStripLevel(TIFF* tiff, const InputType& input) {
  auto dims = input.GetDimensions();

  TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, tile_size_.height);

  // Write strips
  for (int y = 0; y < dims.GetHeight(); y += tile_size_.height) {
    int actual_height = std::min(tile_size_.height, dims.GetHeight() - y);

    Tile tile = input.GetTile(0, y, dims.GetWidth(), actual_height);

    // Convert to bytes regardless of pixel type for correct serialization
    std::vector<uint8_t> strip_data = tile.CreatePaddedDataAsBytes();

    if (TIFFWriteEncodedStrip(tiff, y / tile_size_.height, strip_data.data(),
                              strip_data.size()) < 0) {
      throw std::runtime_error("Failed to write TIFF strip");
    }
  }
}

template <typename InputType>
TileData TiffSink::AssembleImageData(const InputType& input) {
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

  // Build list of tile coordinates
  std::vector<std::tuple<int, int, int, int>> tile_coords;
  for (int y = 0; y < dims.GetHeight(); y += tile_height) {
    for (int x = 0; x < dims.GetWidth(); x += tile_width) {
      int actual_width = std::min(tile_width, dims.GetWidth() - x);
      int actual_height = std::min(tile_height, dims.GetHeight() - y);
      tile_coords.push_back({x, y, actual_width, actual_height});
    }
  }

  // Helper lambda to assemble tiles into a typed vector
  auto assemble_typed = [&]<typename T>() -> TileData {
    size_t num_elements = static_cast<size_t>(dims.GetWidth()) *
                          static_cast<size_t>(dims.GetHeight()) *
                          static_cast<size_t>(dims.channels);
    std::vector<T> image_data(num_elements);

    // Check if we should parallelize
    if (ShouldParallelize(tile_coords.size())) {
      // Parallel tile fetching
      auto& pool = GetThreadPool();
      auto futures = pool.submit_blocks(
          0, tile_coords.size(),
          [&input, &tile_coords](size_t start, size_t end) {
            std::vector<Tile> local_tiles;
            local_tiles.reserve(end - start);

            for (size_t i = start; i < end; ++i) {
              const auto& coord = tile_coords[i];
              int x = std::get<0>(coord);
              int y = std::get<1>(coord);
              int width = std::get<2>(coord);
              int height = std::get<3>(coord);
              local_tiles.push_back(input.GetTile(x, y, width, height));
            }

            return local_tiles;
          });

      // Collect and copy tiles to image data
      for (auto& future : futures) {
        auto tiles = future.get();
        for (auto& tile : tiles) {
          const auto& tile_data =
              std::get<std::vector<T>>(tile.GetVariantData());
          // Copy row by row for better cache performance
          for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
            const T* src_row =
                tile_data.data() + tile_y * tile.width * tile.channels;
            T* dst_row =
                image_data.data() +
                ((tile.y + tile_y) * dims.GetWidth() + tile.x) * dims.channels;
            std::copy(src_row, src_row + tile.width * tile.channels, dst_row);
          }
        }
      }
    } else {
      // Serial path for small images
      for (int y = 0; y < dims.GetHeight(); y += tile_height) {
        for (int x = 0; x < dims.GetWidth(); x += tile_width) {
          int actual_width = std::min(tile_width, dims.GetWidth() - x);
          int actual_height = std::min(tile_height, dims.GetHeight() - y);

          // Get tile from input
          Tile tile = input.GetTile(x, y, actual_width, actual_height);
          const auto& tile_data =
              std::get<std::vector<T>>(tile.GetVariantData());

          // Copy row by row for better cache performance
          for (int tile_y = 0; tile_y < tile.height; ++tile_y) {
            const T* src_row =
                tile_data.data() + tile_y * tile.width * tile.channels;
            T* dst_row =
                image_data.data() +
                ((tile.y + tile_y) * dims.GetWidth() + tile.x) * dims.channels;
            std::copy(src_row, src_row + tile.width * tile.channels, dst_row);
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

template <typename InputType>
TileData TiffSink::AssembleDownsampledData(const InputType& input,
                                           int downsample_factor,
                                           const ImageInfo& target_dims) {
  auto input_dims = input.GetDimensions();

  // Get tile size for fetching from input
  auto ideal_tile_size = input.GetIdealTileSize();
  int tile_width = std::min(ideal_tile_size.width, input_dims.GetWidth());
  int tile_height = std::min(ideal_tile_size.height, input_dims.GetHeight());

  if (tile_width <= 0)
    tile_width = 256;
  if (tile_height <= 0)
    tile_height = 256;

  // Helper lambda to downsample into a typed vector
  auto downsample_typed = [&]<typename T>() -> TileData {
    size_t num_elements = static_cast<size_t>(target_dims.GetWidth()) *
                          static_cast<size_t>(target_dims.GetHeight()) *
                          static_cast<size_t>(target_dims.channels);
    std::vector<T> output_data(num_elements);

    // For each output pixel, fetch and average the corresponding input region
    // Process in blocks to improve cache locality
    int block_size = 64;  // Process output in 64x64 blocks

    for (int block_y = 0; block_y < target_dims.GetHeight();
         block_y += block_size) {
      for (int block_x = 0; block_x < target_dims.GetWidth();
           block_x += block_size) {
        int block_end_x =
            std::min(block_x + block_size, target_dims.GetWidth());
        int block_end_y =
            std::min(block_y + block_size, target_dims.GetHeight());

        // Calculate input region for this block
        int in_x_start = block_x * downsample_factor;
        int in_y_start = block_y * downsample_factor;
        int in_x_end = block_end_x * downsample_factor;
        int in_y_end = block_end_y * downsample_factor;

        // Clamp to input dimensions
        in_x_end = std::min(in_x_end, input_dims.GetWidth());
        in_y_end = std::min(in_y_end, input_dims.GetHeight());

        // Fetch the input region for this block
        Tile input_tile =
            input.GetTile(in_x_start, in_y_start, in_x_end - in_x_start,
                          in_y_end - in_y_start);

        // Process each output pixel in this block
        const auto& tile_data =
            std::get<std::vector<T>>(input_tile.GetVariantData());
        using AccumType =
            std::conditional_t<std::is_floating_point_v<T>, T, int32_t>;

        for (int out_y = block_y; out_y < block_end_y; ++out_y) {
          for (int out_x = block_x; out_x < block_end_x; ++out_x) {
            // Calculate the input region for this output pixel (relative to
            // tile)
            int local_x_start = (out_x * downsample_factor) - in_x_start;
            int local_y_start = (out_y * downsample_factor) - in_y_start;
            int local_x_end =
                std::min(local_x_start + downsample_factor, input_tile.width);
            int local_y_end =
                std::min(local_y_start + downsample_factor, input_tile.height);

            // Average the pixels in the region for each channel
            for (int c = 0; c < target_dims.channels; ++c) {
              AccumType sum = 0;
              int count = 0;

              for (int dy = local_y_start; dy < local_y_end; ++dy) {
                for (int dx = local_x_start; dx < local_x_end; ++dx) {
                  size_t input_index =
                      (dy * input_tile.width + dx) * target_dims.channels + c;
                  sum += static_cast<AccumType>(tile_data[input_index]);
                  count++;
                }
              }

              // Write directly to output vector
              T averaged = static_cast<T>(count > 0 ? sum / count : 0);
              size_t output_index = (out_y * target_dims.GetWidth() + out_x) *
                                        target_dims.channels +
                                    c;
              output_data[output_index] = averaged;
            }
          }
        }
      }
    }

    return TileData(std::move(output_data));
  };

  // Dispatch based on pixel type
  switch (target_dims.pixel_type) {
    case PixelType::kUInt8:
      return downsample_typed.template operator()<uint8_t>();
    case PixelType::kUInt16:
      return downsample_typed.template operator()<uint16_t>();
    case PixelType::kFloat32:
      return downsample_typed.template operator()<float>();
    default:
      throw std::runtime_error(
          "Unsupported pixel type in AssembleDownsampledData");
  }
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SINKS_TIFF_SINK_H_

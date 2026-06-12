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
 * @file openslide_source.h
 * @brief OpenSlide whole slide image source implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the OpenSlideSource and OpenSlideLevelView classes which
 * provide multi-level pyramid access to whole slide images through the
 * OpenSlide library. It supports various formats (SVS, MRXS, NDPI, etc.) with
 * efficient level-based tile access.
 *
 * @note Thread Safety: OpenSlide is not thread-safe, so this implementation
 * uses a std::mutex to protect all OpenSlide API calls. Multiple threads can
 * safely call methods on the same OpenSlideSource instance.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SOURCES_OPENSLIDE_SOURCE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SOURCES_OPENSLIDE_SOURCE_H_

#include <array>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include "fim/pipeline.h"
#include "fim/types.h"
#include "openslide/openslide.h"

namespace fim {

namespace fs = std::filesystem;

// Forward declaration
class OpenSlideSource;

/**
 * @brief Level-specific view of a whole slide image pyramid.
 *
 * This class provides access to a specific pyramid level of a whole slide
 * image. It acts as a standard fimage source (inheriting from SourceBase) and
 * can be used with all fimage operators like Crop, Downsample, etc.
 *
 * Coordinates used in GetTile() and operators are in the selected level's
 * coordinate system. Each level has independent dimensions and coordinates.
 *
 * @note Instances are created via OpenSlideSource::LevelView() and hold a
 * shared reference to the underlying OpenSlide handle and mutex, ensuring the
 * handle stays alive for the lifetime of the view. This allows safe creation
 * from temporary sources.
 * @note Thread-safe: Multiple threads can safely call GetTile() on the same
 * instance.
 */
class OpenSlideLevelView : public SourceBase<OpenSlideLevelView> {
 public:
  /**
   * @brief Gets the dimensions of this pyramid level.
   *
   * Returns the width, height, and number of channels for the selected level.
   *
   * @return ImageInfo containing the level dimensions
   * @throw std::runtime_error if level metadata cannot be retrieved
   */
  ImageInfo GetDimensions() const;

  /**
   * @brief Gets the ideal tile size for processing this level.
   *
   * Returns a reasonable tile size for efficient tile-based processing.
   * OpenSlide doesn't expose native tile sizes, so this returns 512x512.
   *
   * @return TileSize containing the ideal tile dimensions
   */
  TileSize GetIdealTileSize() const;

  /**
   * @brief Gets a tile from this pyramid level.
   *
   * Retrieves a rectangular region of the pyramid level as a tile.
   * Coordinates are in this level's coordinate system.
   *
   * @param x X coordinate of the top-left corner (level coordinates)
   * @param y Y coordinate of the top-left corner (level coordinates)
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @param rgb If true, output RGB (3 channels); if false, output RGBA (4
   * channels)
   * @return Tile containing the requested image data
   * @throw std::runtime_error if tile cannot be read or converted
   */
  Tile GetTile(int x, int y, int width, int height, bool rgb = true) const;

 private:
  friend class OpenSlideSource;

  /**
   * @brief Private constructor - use OpenSlideSource::LevelView().
   *
   * @param handle Shared pointer to OpenSlide handle (keeps handle alive)
   * @param mutex Shared pointer to mutex for thread safety
   * @param level Pyramid level index
   * @param dimensions Dimensions of this level
   * @param downsample Downsample factor for this level
   * @param channels Number of channels
   * @param memory_format Memory layout format
   * @param rgb If true, output RGB (3 channels); if false, output RGBA (4
   * channels)
   */
  OpenSlideLevelView(std::shared_ptr<openslide_t> handle,
                     std::shared_ptr<std::mutex> mutex, int level,
                     Dimensions dimensions, double downsample, int channels,
                     DataLayout memory_format, bool rgb);

  std::shared_ptr<openslide_t>
      handle_;                         ///< Shared ownership of OpenSlide handle
  std::shared_ptr<std::mutex> mutex_;  ///< Shared mutex for thread safety
  int level_;                          ///< Pyramid level index
  Dimensions dimensions_;              ///< Cached level dimensions
  double downsample_;                  ///< Cached downsample factor
  int channels_;                       ///< Cached channel count
  DataLayout memory_format_;           ///< Cached memory layout
  bool rgb_;  ///< Whether to output RGB (true) or RGBA (false)
};

/**
 * @brief Whole slide image source with multi-level pyramid support.
 *
 * This class provides access to whole slide images (WSI) through the OpenSlide
 * library. It supports various formats including SVS, MRXS, NDPI, and others.
 *
 * The class owns an openslide_t handle and provides metadata access methods
 * as well as level-specific views for tile-based processing.
 *
 * @code
 * // Open a whole slide image
 * auto source = fim::OpenSlideSource::Create("slide.svs");
 *
 * // Query metadata
 * std::cout << "Levels: " << source.GetLevelCount() << std::endl;
 * auto mpp = source.GetMpp();
 * std::cout << "MPP: " << mpp[0] << " x " << mpp[1] << std::endl;
 *
 * // Create level view and extract tile
 * auto level2 = source.LevelView(2);
 * auto tile = level2.GetTile(1000, 2000, 512, 512);
 *
 * // Use with fimage pipeline
 * fim::Image(source.LevelView(1))
 *     .Crop(500, 500, 2048, 2048)
 *     .Downsample(2)
 *     .Render(fim::TiffSink::Create("output.tiff"));
 * @endcode
 *
 * @note Use Create() factory method to instantiate.
 * @note Thread-safe: All methods are protected by an internal mutex.
 */
class OpenSlideSource {
 public:
  /**
   * @brief Creates an OpenSlideSource from a whole slide image file.
   *
   * Opens the slide using OpenSlide which automatically detects the format
   * and selects the appropriate reader.
   *
   * @param filename Path to the whole slide image file
   * @return OpenSlideSource instance
   * @throw std::runtime_error if the file cannot be opened or format is
   * unsupported
   */
  static OpenSlideSource Create(const fs::path& filename);

  /**
   * @brief Destructor.
   */
  ~OpenSlideSource() = default;

  /**
   * @brief Deleted copy constructor.
   *
   * OpenSlideSource cannot be copied to ensure clear ownership of the
   * openslide_t handle.
   */
  OpenSlideSource(const OpenSlideSource&) = delete;

  /**
   * @brief Deleted copy assignment operator.
   */
  OpenSlideSource& operator=(const OpenSlideSource&) = delete;

  /**
   * @brief Move constructor.
   *
   * Transfers ownership of the openslide_t handle to the new instance.
   *
   * @param other OpenSlideSource to move from
   */
  OpenSlideSource(OpenSlideSource&& other) noexcept = default;

  /**
   * @brief Move assignment operator.
   *
   * Transfers ownership of the openslide_t handle to this instance.
   *
   * @param other OpenSlideSource to move from
   * @return Reference to this instance
   */
  OpenSlideSource& operator=(OpenSlideSource&& other) noexcept = default;

  /**
   * @brief Creates a level view for the specified pyramid level.
   *
   * Returns an OpenSlideLevelView that acts as a standard fimage source
   * for the selected level. The view uses level-specific coordinates.
   *
   * @param level Pyramid level index (0 = highest resolution)
   * @param rgb If true, output RGB (3 channels); if false, output RGBA (4
   * channels)
   * @return OpenSlideLevelView for the specified level
   * @throw std::out_of_range if level is invalid
   */
  OpenSlideLevelView LevelView(int level, bool rgb = true) const;

  /**
   * @brief Gets the number of pyramid levels.
   *
   * @return Number of pyramid levels (0-indexed)
   */
  int GetLevelCount() const;

  /**
   * @brief Gets the spatial dimensions of a specific pyramid level.
   *
   * Returns only the width and height. Use GetNumChannels() and
   * GetMemoryFormat() for channel and layout information.
   *
   * @param level Pyramid level index
   * @return Dimensions containing width and height
   * @throw std::out_of_range if level is invalid
   */
  Dimensions GetLevelDimensions(int level) const;

  /**
   * @brief Gets the number of color channels in the slide.
   *
   * The channel count is constant across all pyramid levels.
   * Returns 3 for RGB or 4 for RGBA depending on configuration.
   *
   * @param rgb If true, return 3 (RGB); if false, return 4 (RGBA)
   * @return Number of color channels
   */
  int GetNumChannels(bool rgb = true) const;

  /**
   * @brief Gets the memory layout format of the slide data.
   *
   * The memory format is constant across all pyramid levels.
   *
   * @return DataLayout specifying the memory organization
   */
  DataLayout GetMemoryFormat() const;

  /**
   * @brief Gets the downsample factor for a specific pyramid level.
   *
   * The downsample factor is relative to level 0 (highest resolution).
   * Level 0 always has a downsample factor of 1.0.
   *
   * @param level Pyramid level index
   * @return Downsample factor (e.g., 4.0 means 1/4 resolution)
   * @throw std::out_of_range if level is invalid
   */
  double GetLevelDownsample(int level) const;

  /**
   * @brief Gets the microns per pixel (MPP) at level 0.
   *
   * Returns the physical calibration of the slide in microns per pixel.
   * The values are for the highest resolution level (level 0).
   *
   * @return Array [mpp_x, mpp_y] in microns per pixel
   */
  std::array<double, 2> GetMpp() const;

  /**
   * @brief Gets the slide format name.
   *
   * Returns the detected file format (e.g., "aperio", "hamamatsu",
   * "generic-tiff").
   *
   * @return Format name string
   */
  std::string GetFormatName() const;

  /**
   * @brief Gets the slide bounds (bounding box of non-empty region).
   *
   * Returns the minimal bounding rectangle from OpenSlide properties
   * (openslide.bounds-x, openslide.bounds-y, openslide.bounds-width,
   * openslide.bounds-height). If bounds are not available, returns the full
   * slide dimensions as (0, 0, width, height) from level 0.
   *
   * @return Array [x, y, width, height] in level 0 coordinates
   */
  std::array<int64_t, 4> GetBounds() const;

  /**
   * @brief Gets all OpenSlide properties as a map.
   *
   * Iterates through all available properties using
   * openslide_get_property_names and returns them as a key-value map.
   *
   * @return Map of property key-value pairs
   */
  std::map<std::string, std::string> GetAllProperties() const;

  /**
   * @brief Gets the names of available associated images.
   *
   * Associated images are supplementary images like label, macro, or thumbnail.
   *
   * @return Vector of associated image names
   */
  std::vector<std::string> GetAssociatedImageNames() const;

  /**
   * @brief Reads an associated image by name.
   *
   * Reads and converts an associated image from ARGB to RGB format.
   *
   * @param name Name of the associated image (e.g., "thumbnail", "macro")
   * @return Tuple of (RGB data vector, width, height)
   * @throw std::runtime_error if image cannot be read
   */
  std::tuple<std::vector<uint8_t>, int, int> ReadAssociatedImage(
      const std::string& name) const;

  /**
   * @brief Gets the underlying openslide_t handle for advanced use.
   *
   * This provides access to the OpenSlide handle for operations
   * not covered by the fimage API (e.g., associated images, properties).
   *
   * @warning The caller must not call openslide_close() on this handle.
   * @note Thread safety: Callers must acquire the mutex before using.
   * @return Pointer to the underlying openslide_t handle
   */
  openslide_t* GetHandle() const { return handle_.get(); }

  /**
   * @brief Gets the mutex protecting the OpenSlide handle.
   *
   * Advanced users can use this to protect multi-operation sequences.
   *
   * @return Reference to the mutex
   */
  std::mutex& GetMutex() const { return *mutex_; }

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param filename Path to the whole slide image file
   */
  explicit OpenSlideSource(const fs::path& filename);

  std::shared_ptr<openslide_t> handle_;  ///< Shared OpenSlide handle
  std::shared_ptr<std::mutex> mutex_;    ///< Shared mutex for thread safety
  fs::path filename_;                    ///< Path to slide file
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SOURCES_OPENSLIDE_SOURCE_H_

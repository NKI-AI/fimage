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
 * @file fastslide_source.h
 * @brief FastSlide whole slide image source implementation for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the FastSlideSource and FastSlideLevelView classes which
 * provide multi-level pyramid access to whole slide images through the
 * fastslide library. It supports various formats (SVS, QPTIFF, MRXS, etc.) with
 * efficient level-based tile access.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_SOURCES_FASTSLIDE_SOURCE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_SOURCES_FASTSLIDE_SOURCE_H_

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "fastslide/slide_reader.h"
#include "fim/pipeline.h"
#include "fim/types.h"

namespace fim {

namespace fs = std::filesystem;

// Forward declaration
class FastSlideSource;

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
 * @note Instances are created via FastSlideSource::LevelView() and hold a
 * shared reference to the underlying reader, ensuring the reader stays alive
 * for the lifetime of the view. This allows safe creation from temporary
 * sources.
 */
class FastSlideLevelView : public SourceBase<FastSlideLevelView> {
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
   * Returns the native tile size from the whole slide image format.
   * This is the optimal size for efficient tile-based processing.
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
   * @return Tile containing the requested image data
   * @throw std::runtime_error if tile cannot be read or converted
   */
  Tile GetTile(int x, int y, int width, int height) const;

 private:
  friend class FastSlideSource;

  /**
   * @brief Private constructor - use FastSlideSource::LevelView().
   *
   * @param reader Shared pointer to the slide reader (keeps reader alive)
   * @param level Pyramid level index
   * @param dimensions Dimensions of this level
   * @param channels Number of channels
   * @param pixel_type Pixel type for this slide's output
   * @param memory_format Memory layout format
   */
  FastSlideLevelView(std::shared_ptr<const fastslide::SlideReader> reader,
                     int level, Dimensions dimensions, int channels,
                     PixelType pixel_type, DataLayout memory_format);

  std::shared_ptr<const fastslide::SlideReader>
      reader_;                ///< Shared ownership of reader
  int level_;                 ///< Pyramid level index
  Dimensions dimensions_;     ///< Cached level dimensions
  int channels_;              ///< Cached channel count
  PixelType pixel_type_;      ///< Cached pixel type
  DataLayout memory_format_;  ///< Cached memory layout
};

/**
 * @brief Whole slide image source with multi-level pyramid support.
 *
 * This class provides access to whole slide images (WSI) through the fastslide
 * library. It supports various formats including SVS, QPTIFF, MRXS, and NDPI.
 *
 * The class owns a fastslide::SlideReader and provides metadata access methods
 * as well as level-specific views for tile-based processing.
 *
 * ## Example 1: Basic metadata access
 * @code
 * // Open a whole slide image
 * auto source = fim::FastSlideSource::Create("slide.svs");
 *
 * // Query metadata
 * std::cout << "Format: " << source.GetFormatName() << std::endl;
 * std::cout << "Levels: " << source.GetLevelCount() << std::endl;
 *
 * auto mpp = source.GetMpp();
 * std::cout << "MPP: " << mpp[0] << " x " << mpp[1] << " µm/px" << std::endl;
 *
 * // Iterate through pyramid levels
 * for (int i = 0; i < source.GetLevelCount(); ++i) {
 *   auto dims = source.GetLevelDimensions(i);
 *   double downsample = source.GetLevelDownsample(i);
 *   std::cout << "Level " << i << ": " << dims.width << "x" << dims.height
 *             << " (downsample: " << downsample << ")" << std::endl;
 * }
 * @endcode
 *
 * ## Example 2: Extract tiles from a specific level
 * @code
 * auto source = fim::FastSlideSource::Create("slide.svs");
 * auto level2 = source.LevelView(2);
 *
 * // Get a 512x512 tile at coordinates (1000, 2000) in level 2's coordinate
 * system auto tile = level2.GetTile(1000, 2000, 512, 512);
 *
 * // Process the tile data
 * const auto& data = tile.GetData();
 * std::cout << "Tile contains " << data.size() << " bytes" << std::endl;
 * @endcode
 *
 * ## Example 3: Using with fimage pipeline operators
 * @code
 * auto source = fim::FastSlideSource::Create("slide.svs");
 *
 * // Create a pipeline: extract region, crop, downsample, and save
 * fim::Image(source.LevelView(1))
 *     .Crop(500, 500, 2048, 2048)
 *     .Downsample(2)
 *     .Render(fim::TiffSink::Create("output.tiff"));
 * @endcode
 *
 * ## Example 4: Multi-resolution processing
 * @code
 * auto source = fim::FastSlideSource::Create("slide.svs");
 *
 * // Process thumbnail (lowest resolution level)
 * int thumbnail_level = source.GetLevelCount() - 1;
 * auto thumbnail = source.LevelView(thumbnail_level);
 * auto dims = thumbnail.GetDimensions();
 * auto thumb_tile = thumbnail.GetTile(0, 0, dims.width, dims.height);
 *
 * // Process high-resolution region
 * auto level0 = source.LevelView(0);
 * auto hires_tile = level0.GetTile(10000, 20000, 1024, 1024);
 * @endcode
 *
 * @note Use Create() factory method to instantiate.
 * @note Thread-safe: FastSlide reader is thread-safe for concurrent tile
 * access.
 */
class FastSlideSource {
 public:
  /**
   * @brief Creates a FastSlideSource from a whole slide image file.
   *
   * Opens the slide using fastslide's reader registry which automatically
   * detects the format and selects the appropriate reader.
   *
   * @param filename Path to the whole slide image file
   * @return FastSlideSource instance
   * @throw std::runtime_error if the file cannot be opened or format is
   * unsupported
   */
  static FastSlideSource Create(const fs::path& filename);

  /**
   * @brief Destructor.
   */
  ~FastSlideSource() = default;

  /**
   * @brief Deleted copy constructor.
   *
   * FastSlideSource cannot be copied to ensure clear ownership of the
   * SlideReader.
   */
  FastSlideSource(const FastSlideSource&) = delete;

  /**
   * @brief Deleted copy assignment operator.
   */
  FastSlideSource& operator=(const FastSlideSource&) = delete;

  /**
   * @brief Move constructor.
   *
   * Transfers ownership of the SlideReader to the new instance.
   *
   * @param other FastSlideSource to move from
   */
  FastSlideSource(FastSlideSource&& other) noexcept = default;

  /**
   * @brief Move assignment operator.
   *
   * Transfers ownership of the SlideReader to this instance.
   *
   * @param other FastSlideSource to move from
   * @return Reference to this instance
   */
  FastSlideSource& operator=(FastSlideSource&& other) noexcept = default;

  /**
   * @brief Creates a level view for the specified pyramid level.
   *
   * Returns a FastSlideLevelView that acts as a standard fimage source
   * for the selected level. The view uses level-specific coordinates.
   *
   * @param level Pyramid level index (0 = highest resolution)
   * @return FastSlideLevelView for the specified level
   * @throw std::out_of_range if level is invalid
   */
  FastSlideLevelView LevelView(int level) const;

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
   *
   * @return Number of color channels (e.g., 3 for RGB, 4 for RGBA)
   */
  int GetNumChannels() const;

  /**
   * @brief Gets the pixel type produced by fastslide's ReadRegion().
   *
   * Determined once during source construction (by probing a tiny region).
   *
   * @return PixelType for this source
   */
  PixelType GetPixelType() const { return pixel_type_; }

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
   * Returns the detected file format (e.g., "SVS", "QPTIFF", "MRXS").
   *
   * @return Format name string
   */
  std::string GetFormatName() const;

  /**
   * @brief Gets the slide bounds (bounding box of non-empty region).
   *
   * Returns the minimal bounding rectangle containing all non-empty tiles
   * in level 0 coordinates. If bounds are not available, returns the full
   * slide dimensions as (0, 0, width, height) from level 0.
   *
   * @return Array [x, y, width, height] in level 0 coordinates
   */
  std::array<int64_t, 4> GetBounds() const;

  /**
   * @brief Gets channel metadata for all channels.
   *
   * Returns detailed metadata for each imaging channel including name,
   * biomarker, color, and acquisition parameters. Only available for
   * multi-channel formats (e.g., QPTIFF with fluorescence).
   *
   * @return Vector of channel metadata (empty for standard RGB slides)
   */
  std::vector<fastslide::ChannelMetadata> GetChannelMetadata() const;

  /**
   * @brief Gets all slide properties as a map.
   *
   * Returns comprehensive metadata including MPP, magnification, scanner
   * information, and format-specific properties.
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
   * @param name Name of the associated image (e.g., "thumbnail", "macro")
   * @return Image data as RGB uint8 vector with dimensions
   * @throw std::runtime_error if image cannot be read
   */
  std::tuple<std::vector<uint8_t>, int, int> ReadAssociatedImage(
      const std::string& name) const;

  /**
   * @brief Gets the underlying SlideReader for advanced use.
   *
   * This provides access to the fastslide::SlideReader for operations
   * not covered by the fimage API (e.g., associated images, metadata).
   *
   * @return Pointer to the underlying SlideReader
   */
  const fastslide::SlideReader* GetReader() const { return reader_.get(); }

 private:
  /**
   * @brief Private constructor - use Create() factory method.
   *
   * @param filename Path to the whole slide image file
   */
  explicit FastSlideSource(const fs::path& filename);

  std::shared_ptr<fastslide::SlideReader> reader_;  ///< Shared slide reader
  fs::path filename_;                               ///< Path to slide file
  DataLayout memory_format_ =
      DataLayout::kChannelsLast;  ///< Output layout from fastslide ReadRegion()
  PixelType pixel_type_ =
      PixelType::kUInt8;  ///< Output dtype from fastslide ReadRegion()
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_SOURCES_FASTSLIDE_SOURCE_H_

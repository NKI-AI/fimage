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
 * @file factory.h
 * @brief Factory functions for creating type-erased pipeline stages.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file provides factory functions for creating PyStage objects from
 * various sources and operators. These functions handle the type erasure
 * process and make it easy to construct pipeline stages from Python.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_PYTHON_FACTORY_H_
#define AIFO_FIMAGE_INCLUDE_FIM_PYTHON_FACTORY_H_

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aifocore/platform/portability.h"
#include "fim/operators/convert_layout.h"
#include "fim/operators/crop.h"
#include "fim/operators/downsample.h"
#include "fim/operators/paste.h"
#include "fim/operators/resize.h"
#include "fim/operators/stack.h"
#include "fim/python/py_stage.h"
#include "fim/python/stage_adapter.h"
#include "fim/python/stage_model.h"
#include "fim/sources/associated_image_source.h"
#include "fim/sources/black_source.h"
#include "fim/sources/fastslide_source.h"
#include "fim/sources/fimage_source.h"
#include "fim/sources/lodepng_png_source.h"
#include "fim/sources/memory_source.h"
#include "fim/sources/tiff_source.h"

#if defined(FIM_WITH_OPENSLIDE)
#include "fim/sources/openslide_source.h"
#endif  // defined(FIM_WITH_OPENSLIDE)

namespace fim {
namespace python {

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

inline TIFF* OpenTiffForRead(const fs::path& filename) {
  FILE* file = aifocore::portable_fopen(filename, "rb");
  if (file == nullptr) {
    return nullptr;
  }
  const std::string name = filename.string();
  TIFF* tiff = TIFFClientOpen(name.c_str(), "r",
                              reinterpret_cast<thandle_t>(file), TiffClientRead,
                              TiffClientWrite, TiffClientSeek, TiffClientClose,
                              TiffClientSize, TiffClientMap, TiffClientUnmap);
  if (tiff == nullptr) {
    aifocore::portable_fclose(file);
    return nullptr;
  }
  return tiff;
}

inline double ConvertResolutionToPixelsPerMm(double value,
                                             uint16_t resolution_unit) {
  if (value <= 0.0) {
    return 0.0;
  }
  switch (resolution_unit) {
    case RESUNIT_INCH:
      return value / 25.4;
    case RESUNIT_CENTIMETER:
      return value / 10.0;
    default:
      return 0.0;
  }
}

inline ImageProperties ReadTiffImageProperties(const fs::path& filename) {
  std::unique_ptr<TIFF, decltype(&TIFFClose)> tif(OpenTiffForRead(filename),
                                                  &TIFFClose);
  if (!tif) {
    throw std::runtime_error("Failed to open TIFF file: " + filename.string());
  }

  int num_pages = 0;
  do {
    ++num_pages;
  } while (TIFFReadDirectory(tif.get()));

  TIFFSetDirectory(tif.get(), 0);
  float x_res_f = 0.0F;
  float y_res_f = 0.0F;
  double x_res = 0.0;
  double y_res = 0.0;
  if (TIFFGetField(tif.get(), TIFFTAG_XRESOLUTION, &x_res_f) != 0) {
    x_res = static_cast<double>(x_res_f);
  }
  if (TIFFGetField(tif.get(), TIFFTAG_YRESOLUTION, &y_res_f) != 0) {
    y_res = static_cast<double>(y_res_f);
  }
  uint16_t resolution_unit = RESUNIT_NONE;
  TIFFGetFieldDefaulted(tif.get(), TIFFTAG_RESOLUTIONUNIT, &resolution_unit);

  ImageProperties props;
  props["num_pages"] = static_cast<int64_t>((num_pages < 0) ? 0 : num_pages);
  props["x_res"] = ConvertResolutionToPixelsPerMm(x_res, resolution_unit);
  props["y_res"] = ConvertResolutionToPixelsPerMm(y_res, resolution_unit);
  return props;
}

}  // namespace

/**
 * @brief Creates a PyStage from a TIFF file source.
 *
 * This factory function creates a TiffSource and wraps it in a PyStage
 * for use in Python bindings.
 *
 * @param filename Path to the TIFF file
 * @return PyStage wrapping a TiffSource
 * @throw std::runtime_error if the file cannot be opened
 */
inline PyStage MakeTiffSource(const fs::path& filename) {
  auto source = TiffSource::Create(filename);
  auto model = std::make_unique<StageModel<TiffSource>>(std::move(source));
  auto props =
      std::make_shared<ImageProperties>(ReadTiffImageProperties(filename));
  return PyStage(std::move(model), std::move(props));
}

/**
 * @brief Creates a PyStage from a TIFF file source at a given directory index.
 *
 * Multi-page TIFFs store each page in a TIFF directory (IFD). This factory
 * selects the requested directory and exposes it as a normal image stage.
 *
 * @param filename Path to the TIFF file
 * @param directory_index TIFF directory (page) index (0-based)
 * @return PyStage wrapping a TiffSource at the requested directory
 * @throw std::runtime_error if the file cannot be opened or directory is
 * invalid
 */
inline PyStage MakeTiffSource(const fs::path& filename, int directory_index) {
  auto source = TiffSource::Create(filename, directory_index);
  auto model = std::make_unique<StageModel<TiffSource>>(std::move(source));
  auto props =
      std::make_shared<ImageProperties>(ReadTiffImageProperties(filename));
  return PyStage(std::move(model), std::move(props));
}

/**
 * @brief Creates a PyStage from a PNG file source.
 *
 * This factory function creates a PngSource and wraps it in a PyStage
 * for use in Python bindings.
 *
 * @param filename Path to the PNG file
 * @return PyStage wrapping a PngSource
 * @throw std::runtime_error if the file cannot be opened or decoded
 */
inline PyStage MakePngSource(const fs::path& filename) {
  auto source = PngSource::Create(filename);
  auto model = std::make_unique<StageModel<PngSource>>(std::move(source));
  return PyStage(std::move(model));
}

/**
 * @brief Creates a PyStage from an FImage file source.
 *
 * This factory function creates an FImageSource and wraps it in a PyStage
 * for use in Python bindings. FImage is a custom format supporting tiling,
 * compression, and efficient random access.
 *
 * @param filename Path to the FImage file
 * @return PyStage wrapping an FImageSource
 * @throw std::runtime_error if the file cannot be opened or is invalid
 */
inline PyStage MakeFimageSource(const fs::path& filename) {
  auto source = FImageSource::Create(filename);
  auto model = std::make_unique<StageModel<FImageSource>>(std::move(source));
  return PyStage(std::move(model));
}

/**
 * @brief Creates a PyStage from a memory source with specified pixel type.
 *
 * This factory function creates a MemorySource from raw image data
 * and wraps it in a PyStage for use in Python bindings.
 *
 * @param data Raw image data (as bytes)
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels
 * @param pixel_type Pixel data type
 * @return PyStage wrapping a MemorySource
 */
inline PyStage MakeMemorySource(std::vector<uint8_t>&& data, int width,
                                int height, int channels,
                                PixelType pixel_type = PixelType::kUInt8,
                                DataLayout layout = DataLayout::kChannelsLast) {
  ImageInfo dims(width, height, channels, pixel_type, layout);
  auto source = MemorySource::Create(std::move(data), dims);
  auto model = std::make_unique<StageModel<MemorySource>>(std::move(source));
  return PyStage(std::move(model));
}

/**
 * @brief Creates a PyStage from typed memory data.
 *
 * This factory function creates a MemorySource from typed image data
 * and wraps it in a PyStage for use in Python bindings.
 *
 * @tparam T The pixel type (uint8_t, uint16_t, or float)
 * @param data Typed image data
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels
 * @return PyStage wrapping a MemorySource
 */
template <typename T>
inline PyStage MakeMemorySourceTyped(
    std::vector<T>&& data, int width, int height, int channels,
    DataLayout layout = DataLayout::kChannelsLast) {
  auto source = MemorySource::CreateTyped(std::move(data), width, height,
                                          channels, layout);
  auto model = std::make_unique<StageModel<MemorySource>>(std::move(source));
  return PyStage(std::move(model));
}

/**
 * @brief Wrapper that owns FastSlideSource and provides level view interface.
 *
 * This class maintains a reference to the FastSlideSource for metadata access
 * and delegates tile operations to the level view. The level view independently
 * maintains shared ownership of the underlying reader, so the reader stays
 * alive as long as either the source or the view exists.
 *
 * NOTE: Does NOT inherit from SourceBase to avoid template recursion issues.
 * Provides the required interface methods for StageModel.
 */
class FastSlideLevelOwner {
 public:
  FastSlideLevelOwner(std::shared_ptr<FastSlideSource> source, int level)
      : source_(std::move(source)), level_view_(source_->LevelView(level)) {}

  ImageInfo GetDimensions() const { return level_view_.GetDimensions(); }

  TileSize GetIdealTileSize() const { return level_view_.GetIdealTileSize(); }

  Tile GetTile(int x, int y, int width, int height) const {
    return level_view_.GetTile(x, y, width, height);
  }

 private:
  std::shared_ptr<FastSlideSource>
      source_;                     ///< Owned source (for metadata access)
  FastSlideLevelView level_view_;  ///< Level view (owns reader independently)
};

/**
 * @brief Context manager for FastSlide whole slide images.
 *
 * This class provides a pythonic context manager interface for working with
 * whole slide images. It maintains a shared FastSlideSource and allows
 * creating multiple level views from the same source.
 *
 * Usage in Python:
 *   with fim.open_fastslide("slide.svs") as slide:
 *       print(f"Levels: {slide.level_count}")
 *       img = slide.at_level(0).crop(...).resize(...)
 */
class FastSlideContext {
 public:
  explicit FastSlideContext(std::shared_ptr<FastSlideSource> source)
      : source_(std::move(source)) {}

  /**
   * @brief Create an Image at a specific pyramid level.
   *
   * @param level Pyramid level (0 = highest resolution)
   * @return PyStage wrapping the level view
   */
  PyStage AtLevel(int level) const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    auto owner = FastSlideLevelOwner(source_, level);
    auto model =
        std::make_unique<StageModel<FastSlideLevelOwner>>(std::move(owner));
    return PyStage(std::move(model));
  }

  /**
   * @brief Close the slide and release resources.
   *
   * Releases the reference to the underlying FastSlideSource. This is called
   * automatically when the context manager exits, but can also be called
   * manually when not using the context manager pattern. Safe to call
   * multiple times.
   *
   * Note: If PyStage objects created via AtLevel() still exist, they maintain
   * their own shared_ptr references, so the underlying resource will only be
   * freed when all references are gone.
   */
  void Close() { source_.reset(); }

  /**
   * @brief Check if the context is still open.
   *
   * @return True if the context is open and can be used
   */
  bool IsOpen() const { return source_ != nullptr; }

  int GetLevelCount() const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetLevelCount();
  }

  std::array<double, 2> GetMpp() const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetMpp();
  }

  std::string GetFormatName() const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetFormatName();
  }

  Dimensions GetLevelDimensions(int level) const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetLevelDimensions(level);
  }

  int GetNumChannels() const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetNumChannels();
  }

  DataLayout GetMemoryFormat() const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetMemoryFormat();
  }

  double GetLevelDownsample(int level) const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetLevelDownsample(level);
  }

  std::array<int64_t, 4> GetBounds() const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetBounds();
  }

  std::vector<fastslide::ChannelMetadata> GetChannelMetadata() const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetChannelMetadata();
  }

  std::map<std::string, std::string> GetAllProperties() const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetAllProperties();
  }

  std::vector<std::string> GetAssociatedImageNames() const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    return source_->GetAssociatedImageNames();
  }

  PyStage GetAssociatedImage(const std::string& name) const {
    if (!source_) {
      throw std::runtime_error("FastSlideContext has been closed");
    }
    auto source = FastSlideAssociatedImageSource::Create(source_, name);
    auto model = std::make_unique<StageModel<FastSlideAssociatedImageSource>>(
        std::move(source));
    return PyStage(std::move(model));
  }

  std::shared_ptr<FastSlideSource> GetSource() const { return source_; }

 private:
  std::shared_ptr<FastSlideSource> source_;
};

/**
 * @brief Opens a FastSlide whole slide image as a context manager.
 *
 * @param filename Path to the whole slide image file
 * @return FastSlideContext that can create level views
 */
inline FastSlideContext OpenFastSlide(const fs::path& filename) {
  auto source =
      std::make_shared<FastSlideSource>(FastSlideSource::Create(filename));
  return FastSlideContext(source);
}

/**
 * @brief Creates a PyStage from a FastSlide whole slide image source.
 *
 * This factory function creates a FastSlideLevelOwner for a specific pyramid
 * level of a whole slide image and wraps it in a PyStage for use in Python
 * bindings. Supports various formats: SVS, QPTIFF, MRXS, etc.
 *
 * The owner ensures the FastSlideSource stays alive as long as the level
 * view is in use, preventing dangling pointer issues.
 *
 * @param filename Path to the whole slide image file
 * @param level Pyramid level to use (default: 0 = highest resolution)
 * @return PyStage wrapping a FastSlideLevelOwner
 * @throw std::runtime_error if the file cannot be opened or level is invalid
 */
inline PyStage MakeFastSlideSource(const fs::path& filename, int level = 0) {
  auto source =
      std::make_shared<FastSlideSource>(FastSlideSource::Create(filename));
  auto owner = FastSlideLevelOwner(source, level);
  auto model =
      std::make_unique<StageModel<FastSlideLevelOwner>>(std::move(owner));
  return PyStage(std::move(model));
}

/**
 * @brief Wrapper that owns OpenSlideSource and provides level view interface.
 *
 * This class maintains a reference to the OpenSlideSource for metadata access
 * and delegates tile operations to the level view. The level view independently
 * maintains shared ownership of the underlying handle and mutex, so the handle
 * stays alive as long as either the source or the view exists.
 *
 * NOTE: Does NOT inherit from SourceBase to avoid template recursion issues.
 * Provides the required interface methods for StageModel.
 */
#if defined(FIM_WITH_OPENSLIDE)
class OpenSlideLevelOwner {
 public:
  OpenSlideLevelOwner(std::shared_ptr<OpenSlideSource> source, int level,
                      bool rgb = true)
      : source_(std::move(source)),
        level_view_(source_->LevelView(level, rgb)) {}

  ImageInfo GetDimensions() const { return level_view_.GetDimensions(); }

  TileSize GetIdealTileSize() const { return level_view_.GetIdealTileSize(); }

  Tile GetTile(int x, int y, int width, int height) const {
    return level_view_.GetTile(x, y, width, height);
  }

 private:
  std::shared_ptr<OpenSlideSource>
      source_;                     ///< Owned source (for metadata access)
  OpenSlideLevelView level_view_;  ///< Level view (owns handle independently)
};

/**
 * @brief Context manager for OpenSlide whole slide images.
 *
 * This class provides a pythonic context manager interface for working with
 * whole slide images. It maintains a shared OpenSlideSource and allows
 * creating multiple level views from the same source.
 *
 * Usage in Python:
 *   with fim.open_openslide("slide.svs") as slide:
 *       print(f"Levels: {slide.level_count}")
 *       img = slide.at_level(0).crop(...).resize(...)
 */
class OpenSlideContext {
 public:
  explicit OpenSlideContext(std::shared_ptr<OpenSlideSource> source,
                            bool rgb = true)
      : source_(std::move(source)), rgb_(rgb) {}

  /**
   * @brief Create an Image at a specific pyramid level.
   *
   * @param level Pyramid level (0 = highest resolution)
   * @return PyStage wrapping the level view
   */
  PyStage AtLevel(int level) const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    auto owner = OpenSlideLevelOwner(source_, level, rgb_);
    auto model =
        std::make_unique<StageModel<OpenSlideLevelOwner>>(std::move(owner));
    return PyStage(std::move(model));
  }

  /**
   * @brief Close the slide and release resources.
   *
   * Releases the reference to the underlying OpenSlideSource. This is called
   * automatically when the context manager exits, but can also be called
   * manually when not using the context manager pattern. Safe to call
   * multiple times.
   *
   * Note: If PyStage objects created via AtLevel() still exist, they maintain
   * their own shared_ptr references, so the underlying resource will only be
   * freed when all references are gone.
   */
  void Close() { source_.reset(); }

  /**
   * @brief Check if the context is still open.
   *
   * @return True if the context is open and can be used
   */
  bool IsOpen() const { return source_ != nullptr; }

  int GetLevelCount() const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetLevelCount();
  }

  std::array<double, 2> GetMpp() const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetMpp();
  }

  std::string GetFormatName() const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetFormatName();
  }

  Dimensions GetLevelDimensions(int level) const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetLevelDimensions(level);
  }

  int GetNumChannels() const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetNumChannels(rgb_);
  }

  DataLayout GetMemoryFormat() const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetMemoryFormat();
  }

  double GetLevelDownsample(int level) const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetLevelDownsample(level);
  }

  std::array<int64_t, 4> GetBounds() const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetBounds();
  }

  std::map<std::string, std::string> GetAllProperties() const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetAllProperties();
  }

  std::vector<std::string> GetAssociatedImageNames() const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    return source_->GetAssociatedImageNames();
  }

  PyStage GetAssociatedImage(const std::string& name) const {
    if (!source_) {
      throw std::runtime_error("OpenSlideContext has been closed");
    }
    auto source = OpenSlideAssociatedImageSource::Create(source_, name);
    auto model = std::make_unique<StageModel<OpenSlideAssociatedImageSource>>(
        std::move(source));
    return PyStage(std::move(model));
  }

  std::shared_ptr<OpenSlideSource> GetSource() const { return source_; }

 private:
  std::shared_ptr<OpenSlideSource> source_;
  bool rgb_;
};
#endif  // defined(FIM_WITH_OPENSLIDE)

/**
 * @brief Context manager for multi-page TIFF images (libtiff).
 *
 * This class mirrors the `open_fastslide` / `open_openslide` style: it provides
 * `level_count`, `level_dimensions`, `properties`, and `at_level(level)`.
 *
 * For TIFF, "levels" correspond to TIFF directories (pages/IFDs). No pyramid
 * downsample semantics are assumed.
 */
class LibtiffContext {
 public:
  explicit LibtiffContext(fs::path filename) : filename_(std::move(filename)) {
    LoadMetadata();
  }

  PyStage AtLevel(int level) const {
    if (!is_open_) {
      throw std::runtime_error("LibtiffContext has been closed");
    }
    if (level < 0 || level >= static_cast<int>(level_dimensions_.size())) {
      throw std::runtime_error("Invalid TIFF directory index: " +
                               std::to_string(level));
    }
    return MakeTiffSource(filename_, level);
  }

  void Close() { is_open_ = false; }

  bool IsOpen() const { return is_open_; }

  int GetLevelCount() const {
    if (!is_open_) {
      throw std::runtime_error("LibtiffContext has been closed");
    }
    return static_cast<int>(level_dimensions_.size());
  }

  Dimensions GetLevelDimensions(int level) const {
    if (!is_open_) {
      throw std::runtime_error("LibtiffContext has been closed");
    }
    if (level < 0 || level >= static_cast<int>(level_dimensions_.size())) {
      throw std::runtime_error("Invalid TIFF directory index: " +
                               std::to_string(level));
    }
    return level_dimensions_[level];
  }

  ImageProperties GetAllProperties() const {
    if (!is_open_) {
      throw std::runtime_error("LibtiffContext has been closed");
    }
    return properties_;
  }

 private:
  void LoadMetadata() {
    if (!std::filesystem::exists(filename_)) {
      throw std::runtime_error("TIFF file does not exist: " +
                               filename_.string());
    }

    std::unique_ptr<TIFF, decltype(&TIFFClose)> tif(OpenTiffForRead(filename_),
                                                    &TIFFClose);
    if (!tif) {
      throw std::runtime_error("Failed to open TIFF file: " +
                               filename_.string());
    }

    // Read page count + dimensions by iterating directories.
    level_dimensions_.clear();
    do {
      uint32_t width = 0;
      uint32_t height = 0;
      TIFFGetField(tif.get(), TIFFTAG_IMAGEWIDTH, &width);
      TIFFGetField(tif.get(), TIFFTAG_IMAGELENGTH, &height);
      level_dimensions_.emplace_back(static_cast<int>(width),
                                     static_cast<int>(height));
    } while (TIFFReadDirectory(tif.get()));

    // Read resolution from the first directory (page 0).
    double x_res = 0.0;
    double y_res = 0.0;
    uint16_t resolution_unit = RESUNIT_NONE;
    TIFFSetDirectory(tif.get(), 0);

    float x_res_f = 0.0F;
    float y_res_f = 0.0F;
    if (TIFFGetField(tif.get(), TIFFTAG_XRESOLUTION, &x_res_f) != 0) {
      x_res = static_cast<double>(x_res_f);
    }
    if (TIFFGetField(tif.get(), TIFFTAG_YRESOLUTION, &y_res_f) != 0) {
      y_res = static_cast<double>(y_res_f);
    }
    TIFFGetFieldDefaulted(tif.get(), TIFFTAG_RESOLUTIONUNIT, &resolution_unit);

    const double x_ppmm =
        ConvertResolutionToPixelsPerMm(x_res, resolution_unit);
    const double y_ppmm =
        ConvertResolutionToPixelsPerMm(y_res, resolution_unit);

    properties_.clear();
    properties_["num_pages"] = static_cast<int64_t>(level_dimensions_.size());
    properties_["x_res"] = x_ppmm;
    properties_["y_res"] = y_ppmm;
  }

  fs::path filename_;
  bool is_open_ = true;
  std::vector<Dimensions> level_dimensions_;
  ImageProperties properties_;
};

/**
 * @brief Opens a multi-page TIFF as a context manager.
 *
 * @param filename Path to the TIFF file
 * @return LibtiffContext that can create directory (page) views
 */
inline LibtiffContext OpenLibtiff(const fs::path& filename) {
  return LibtiffContext(filename);
}

/**
 * @brief Opens an OpenSlide whole slide image as a context manager.
 *
 * @param filename Path to the whole slide image file
 * @param rgb If true, output RGB (3 channels); if false, output RGBA (4
 * channels)
 * @return OpenSlideContext that can create level views
 */
#if defined(FIM_WITH_OPENSLIDE)
inline OpenSlideContext OpenOpenSlide(const fs::path& filename,
                                      bool rgb = true) {
  auto source =
      std::make_shared<OpenSlideSource>(OpenSlideSource::Create(filename));
  return OpenSlideContext(source, rgb);
}
#endif  // defined(FIM_WITH_OPENSLIDE)

/**
 * @brief Creates a PyStage from an OpenSlide whole slide image source.
 *
 * This factory function creates an OpenSlideLevelOwner for a specific pyramid
 * level of a whole slide image and wraps it in a PyStage for use in Python
 * bindings. Supports various formats: SVS, MRXS, NDPI, etc.
 *
 * The owner ensures the OpenSlideSource stays alive as long as the level
 * view is in use, preventing dangling pointer issues.
 *
 * @param filename Path to the whole slide image file
 * @param level Pyramid level to use (default: 0 = highest resolution)
 * @param rgb If true, output RGB (3 channels); if false, output RGBA (4
 * channels)
 * @return PyStage wrapping an OpenSlideLevelOwner
 * @throw std::runtime_error if the file cannot be opened or level is invalid
 */
#if defined(FIM_WITH_OPENSLIDE)
inline PyStage MakeOpenSlideSource(const fs::path& filename, int level = 0,
                                   bool rgb = true) {
  auto source =
      std::make_shared<OpenSlideSource>(OpenSlideSource::Create(filename));
  auto owner = OpenSlideLevelOwner(source, level, rgb);
  auto model =
      std::make_unique<StageModel<OpenSlideLevelOwner>>(std::move(owner));
  return PyStage(std::move(model));
}
#endif  // defined(FIM_WITH_OPENSLIDE)

/**
 * @brief Applies a Crop operator to a PyStage.
 *
 * This function creates a new PyStage with a Crop operator applied
 * to the input stage. It reuses the C++ CRTP Crop operator by wrapping
 * the type-erased stage in a PyStageAdapter.
 *
 * @param input The input PyStage to crop
 * @param x X coordinate of the crop region
 * @param y Y coordinate of the crop region
 * @param width Width of the crop region
 * @param height Height of the crop region
 * @return New PyStage with the Crop operator applied
 */
inline PyStage ApplyCrop(const PyStage& input, int x, int y, int width,
                         int height) {
  PyStageAdapter adapter(input.GetStagePtr());
  auto crop = Crop<PyStageAdapter>(adapter, x, y, width, height);
  auto model =
      std::make_unique<StageModel<Crop<PyStageAdapter>>>(std::move(crop));
  return PyStage(std::move(model));
}

/**
 * @brief Applies a Downsample operator to a PyStage.
 *
 * This function creates a new PyStage with a Downsample operator applied
 * to the input stage. It reuses the C++ CRTP Downsample operator by wrapping
 * the type-erased stage in a PyStageAdapter.
 *
 * @param input The input PyStage to downsample
 * @param factor The downsampling factor
 * @return New PyStage with the Downsample operator applied
 */
inline PyStage ApplyDownsample(const PyStage& input, int factor) {
  PyStageAdapter adapter(input.GetStagePtr());
  auto downsample = Downsample<PyStageAdapter>(adapter, factor);
  auto model = std::make_unique<StageModel<Downsample<PyStageAdapter>>>(
      std::move(downsample));
  return PyStage(std::move(model));
}

/**
 * @brief Applies a Resize operator to a PyStage.
 *
 * This function creates a new PyStage with a Resize operator applied
 * to the input stage. It reuses the C++ CRTP Resize operator by wrapping
 * the type-erased stage in a PyStageAdapter.
 *
 * @param input The input PyStage to resize
 * @param target_width Target output width
 * @param target_height Target output height
 * @param kernel Resampling kernel to use
 * @param box Optional box parameter for subpixel-accurate source region
 * @return New PyStage with the Resize operator applied
 */
inline PyStage ApplyResize(
    const PyStage& input, int target_width, int target_height,
    resize::KernelType kernel = resize::KernelType::kLanczos3,
    std::optional<resize::Box> box = std::nullopt) {
  PyStageAdapter adapter(input.GetStagePtr());
  auto resize_op =
      Resize<PyStageAdapter>(adapter, target_width, target_height, kernel, box);
  auto model = std::make_unique<StageModel<Resize<PyStageAdapter>>>(
      std::move(resize_op));
  return PyStage(std::move(model));
}

/**
 * @brief Stacks multiple PyStages along the channel axis.
 *
 * This function creates a new PyStage that combines multiple input stages
 * by concatenating their channels. It reuses the C++ CRTP Stack operator
 * by wrapping each type-erased stage in a PyStageAdapter.
 *
 * @param inputs Vector of PyStages to stack
 * @param axis Stacking axis (currently only "bands" for channel concatenation)
 * @return New PyStage with the Stack operator applied
 * @throw std::runtime_error if inputs have incompatible dimensions
 */
inline PyStage ApplyStack(const std::vector<PyStage>& inputs,
                          const std::string& axis = "bands") {
  if (inputs.empty()) {
    throw std::runtime_error("Cannot stack zero images");
  }

  if (axis != "bands") {
    throw std::runtime_error("Only 'bands' axis is currently supported");
  }

  // Wrap all input PyStages in PyStageAdapters
  std::vector<PyStageAdapter> adapters;
  adapters.reserve(inputs.size());
  for (const auto& input : inputs) {
    adapters.emplace_back(input.GetStagePtr());
  }

  auto stack = Stack<PyStageAdapter>(std::move(adapters));
  auto model =
      std::make_unique<StageModel<Stack<PyStageAdapter>>>(std::move(stack));
  return PyStage(std::move(model));
}

/**
 * @brief Pastes a foreground PyStage onto a background PyStage.
 *
 * This function creates a new PyStage that composites the foreground image
 * onto the background image at the specified position. It reuses the C++ CRTP
 * Paste operator by wrapping both type-erased stages in PyStageAdapters.
 *
 * @param background The background PyStage
 * @param foreground The foreground PyStage to paste
 * @param x X coordinate where foreground top-left will be placed
 * @param y Y coordinate where foreground top-left will be placed
 * @return New PyStage with the Paste operator applied
 * @throw std::invalid_argument if channels don't match or foreground is out of
 * bounds
 */
inline PyStage ApplyPaste(const PyStage& background, const PyStage& foreground,
                          int x, int y) {
  PyStageAdapter bg_adapter(background.GetStagePtr());
  PyStageAdapter fg_adapter(foreground.GetStagePtr());

  auto paste =
      Paste<PyStageAdapter, PyStageAdapter>(bg_adapter, fg_adapter, x, y);
  auto model =
      std::make_unique<StageModel<Paste<PyStageAdapter, PyStageAdapter>>>(
          std::move(paste));
  return PyStage(std::move(model));
}

/**
 * @brief Converts the memory layout of a PyStage lazily.
 */
inline PyStage ApplyConvertLayout(const PyStage& input, DataLayout layout) {
  PyStageAdapter adapter(input.GetStagePtr());
  auto op = ConvertLayout<PyStageAdapter>(adapter, layout);
  auto model = std::make_unique<StageModel<ConvertLayout<PyStageAdapter>>>(
      std::move(op));
  return PyStage(std::move(model));
}

}  // namespace python
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_PYTHON_FACTORY_H_

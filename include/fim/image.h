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
 * @file image.h
 * @brief Main image class and convenience functions for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the main Image class template and convenience functions
 * for creating and working with images in the fim image processing pipeline.
 * It provides a high-level interface for common image operations and source
 * creation.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_IMAGE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_IMAGE_H_

#include <filesystem>
#include <utility>

#include "fim/operators/convert_layout.h"
#include "fim/operators/crop.h"
#include "fim/operators/downsample.h"
#include "fim/operators/resize.h"
#include "fim/sinks/lodepng_png_sink.h"
#include "fim/sinks/memory_sink.h"
#include "fim/sinks/tiff_sink.h"
#include "fim/sources/fastslide_source.h"
#include "fim/sources/lodepng_png_source.h"
#include "fim/sources/memory_source.h"
#include "fim/sources/tiff_source.h"

namespace fim {

/**
 * @brief Main Image class template that wraps pipeline stages.
 *
 * This class provides a high-level interface for image processing operations
 * by wrapping pipeline sources and operators. It supports method chaining
 * for building complex processing pipelines and provides convenient methods
 * for common operations like cropping, downsampling, and rendering.
 *
 * The class uses the CRTP pattern internally but provides a simpler interface
 * for end users. Each operation returns a new Image instance wrapping the
 * appropriate operator or source.
 *
 * @tparam SourceType The type of the underlying source or operator
 *
 * @code
 * // Example usage:
 * auto image = fim::CreateTiffImage("input.tiff");
 * image.Crop(100, 100, 500, 500)
 *      .Downsample(2)
 *      .Render(fim::LodePngSink("output.png"));
 * @endcode
 */
template <typename SourceType>
class Image {
 public:
  /**
   * @brief Constructs an Image from a source object.
   *
   * This constructor takes ownership of the source object and wraps it
   * in the Image interface. The source object is moved to avoid unnecessary
   * copying.
   *
   * @param source The source object to wrap (moved)
   */
  explicit Image(SourceType&& source) : source_(std::move(source)) {}

  /**
   * @brief Constructs an Image from a filename.
   *
   * This constructor is available for sources that support construction
   * from a filename. It creates the appropriate source type and wraps it
   * in the Image interface.
   *
   * @param filename Path to the image file
   */
  explicit Image(const fs::path& filename) : source_(filename) {}

  /**
   * @brief Applies a crop operation to the image.
   *
   * This method creates a new Image instance with a crop operator applied
   * to the current source. The crop region is specified by the top-left
   * corner and dimensions. The source is moved into the new operator to
   * ensure proper lifetime management when chaining operations.
   *
   * Note: This method has rvalue ref-qualifier (&&), so it consumes the
   * Image. Call on temporaries or use std::move for named variables.
   *
   * @param x X coordinate of the top-left corner of the crop region
   * @param y Y coordinate of the top-left corner of the crop region
   * @param width Width of the crop region
   * @param height Height of the crop region
   * @return New Image instance with the crop operator applied
   */
  auto Crop(int x, int y, int width, int height) && {
    return Image<fim::Crop<SourceType>>(
        fim::Crop<SourceType>(std::move(source_), x, y, width, height));
  }

  /**
   * @brief Applies a downsample operation to the image.
   *
   * This method creates a new Image instance with a downsample operator
   * applied to the current source. The downsample factor determines how
   * much the image size is reduced. The source is moved into the new
   * operator to ensure proper lifetime management when chaining operations.
   *
   * Note: This method has rvalue ref-qualifier (&&), so it consumes the
   * Image. Call on temporaries or use std::move for named variables.
   *
   * @param factor The downsampling factor (must be positive)
   * @return New Image instance with the downsample operator applied
   */
  auto Downsample(int factor) && {
    return Image<fim::Downsample<SourceType>>(
        fim::Downsample<SourceType>(std::move(source_), factor));
  }

  /**
   * @brief Applies a resize operation to the image.
   *
   * This method creates a new Image instance with a resize operator applied
   * to the current source. The resize operator performs high-quality resampling
   * to the specified target dimensions using advanced kernels. The source is
   * moved into the new operator to ensure proper lifetime management when
   * chaining operations.
   *
   * Note: This method has rvalue ref-qualifier (&&), so it consumes the
   * Image. Call on temporaries or use std::move for named variables.
   *
   * @param target_width Target output width in pixels
   * @param target_height Target output height in pixels
   * @param kernel Resampling kernel to use (default: Lanczos3)
   * @param box Optional box parameter specifying source region in
   * floating-point pixel coordinates. If not provided, uses the entire input.
   * @return New Image instance with the resize operator applied
   */
  auto Resize(int target_width, int target_height,
              resize::KernelType kernel = resize::KernelType::kLanczos3,
              std::optional<resize::Box> box = std::nullopt) && {
    return Image<fim::Resize<SourceType>>(fim::Resize<SourceType>(
        std::move(source_), target_width, target_height, kernel, box));
  }

  /**
   * @brief Converts the image's memory layout (channels-first/last).
   *
   * This inserts a lazy ConvertLayout operator into the pipeline. The operation
   * only reorders values when tiles are requested by a sink.
   *
   * @param layout Target DataLayout
   * @return New Image instance with ConvertLayout applied
   */
  auto ToLayout(DataLayout layout) && {
    return Image<fim::ConvertLayout<SourceType>>(
        fim::ConvertLayout<SourceType>(std::move(source_), layout));
  }

  /**
   * @brief Renders the image to a sink.
   *
   * This method processes the entire image pipeline and writes the result
   * to the specified sink. The sink determines the output format and
   * destination. Uses perfect forwarding to preserve move semantics for
   * temporary sinks.
   *
   * @tparam SinkType The type of the sink (e.g., LodePngSink, TiffSink,
   * MemorySink)
   * @param sink The sink to render to (passed by reference or rvalue)
   */
  template <typename SinkType>
  void Render(SinkType&& sink) {
    std::forward<SinkType>(sink).Render(source_);
  }

  /**
   * @brief Gets the underlying source for advanced use.
   *
   * This method provides access to the underlying source or operator
   * for advanced operations that are not covered by the high-level
   * Image interface.
   *
   * @return Const reference to the underlying source
   */
  const SourceType& GetSource() const { return source_; }

 private:
  SourceType source_;  ///< The underlying source or operator
};

/**
 * @brief Creates an Image from a TIFF file.
 *
 * This convenience function creates an Image instance wrapping a TiffSource
 * for the specified file. It provides a simple way to start processing
 * TIFF images.
 *
 * @param filename Path to the TIFF file
 * @return Image instance wrapping a TiffSource
 * @throw std::runtime_error if the TIFF file cannot be opened
 */
inline Image<TiffSource> CreateTiffImage(const fs::path& filename) {
  return Image<TiffSource>(TiffSource::Create(filename));
}

/**
 * @brief Creates an Image from a PNG file.
 *
 * This convenience function creates an Image instance wrapping a PngSource
 * for the specified file. It provides a simple way to start processing
 * PNG images.
 *
 * @param filename Path to the PNG file
 * @return Image instance wrapping a PngSource
 * @throw std::runtime_error if the PNG file cannot be opened or decoded
 */
inline Image<PngSource> CreatePngImage(const fs::path& filename) {
  return Image<PngSource>(PngSource::Create(filename));
}

/**
 * @brief Creates an Image from a whole slide image at a specific pyramid level.
 *
 * This convenience function creates an Image instance wrapping a
 * FastSlideLevelView for the specified file and pyramid level. It provides
 * a simple way to start processing whole slide images (WSI) with multi-level
 * pyramid support.
 *
 * The returned Image uses the selected level's coordinate system for all
 * operations (Crop, GetTile, etc.).
 *
 * @param filename Path to the whole slide image file (SVS, QPTIFF, MRXS, etc.)
 * @param level Pyramid level to use (default: 0 = highest resolution)
 * @return Image instance wrapping a FastSlideLevelView
 * @throw std::runtime_error if the slide file cannot be opened or level is
 * invalid
 *
 * @code
 * // Open slide at level 0 (highest resolution)
 * auto image = fim::CreateFastSlideImage("slide.svs");
 *
 * // Open slide at level 2 (lower resolution)
 * auto image_l2 = fim::CreateFastSlideImage("slide.svs", 2);
 *
 * // Use with fimage pipeline
 * fim::CreateFastSlideImage("slide.svs", 1)
 *     .Crop(1000, 1000, 2048, 2048)
 *     .Downsample(2)
 *     .Render(fim::TiffSink::Create("output.tiff"));
 * @endcode
 */
inline Image<FastSlideLevelView> CreateFastSlideImage(const fs::path& filename,
                                                      int level = 0) {
  auto source = FastSlideSource::Create(filename);
  return Image<FastSlideLevelView>(source.LevelView(level));
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_IMAGE_H_

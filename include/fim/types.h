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
 * @file types.h
 * @brief Core type definitions for the fim image processing library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the fundamental data structures used throughout the fim
 * library for image processing, including tile representation, image
 * dimensions, and tile size specifications.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_TYPES_H_
#define AIFO_FIMAGE_INCLUDE_FIM_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "fim/pixel_types.h"

namespace fim {

// Inline definition of GetPixelTypeSize for use in Tile methods
// (avoiding circular dependency with pixel_type_traits.h)
constexpr size_t GetPixelTypeSize(PixelType type) {
  switch (type) {
    case PixelType::kUInt8:
      return 1;
    case PixelType::kUInt16:
      return 2;
    case PixelType::kFloat32:
      return 4;
    default:
      throw std::invalid_argument("Unknown pixel type");
  }
}

/**
 * @brief Represents spatial dimensions of an image (width and height only).
 *
 * This structure contains only the spatial dimensions, separating them
 * from channel count and memory layout for clearer API semantics.
 */
struct Dimensions {
  int width = 0;   ///< Width of the image in pixels
  int height = 0;  ///< Height of the image in pixels

  /**
   * @brief Default constructor creates zero-dimensional image.
   */
  Dimensions() = default;

  /**
   * @brief Constructs dimensions with specified values.
   *
   * @param w Width in pixels
   * @param h Height in pixels
   */
  Dimensions(int w, int h) : width(w), height(h) {}
};

/**
 * @brief Variant type for storing different pixel data types.
 */
using TileData = std::variant<std::vector<uint8_t>, std::vector<uint16_t>,
                              std::vector<float>>;

/**
 * @brief Represents a rectangular tile of image data with lazy evaluation
 * support.
 *
 * A tile is a rectangular region of an image that can be processed
 * independently. This structure contains both the tile's position and
 * dimensions, as well as the actual pixel data (materialized lazily) and
 * metadata about padding requirements.
 *
 * The tile supports two modes:
 * - Eager: Data is immediately allocated (backward compatible)
 * - Lazy: Data is produced on-demand when GetData() is first called
 *
 * The tile now supports multiple pixel types (uint8, uint16, float32) through
 * std::variant storage for type-safe operations.
 */
struct Tile {
  int x = 0;         ///< X coordinate of the tile's top-left corner
  int y = 0;         ///< Y coordinate of the tile's top-left corner
  int width = 0;     ///< Width of the tile in pixels
  int height = 0;    ///< Height of the tile in pixels
  int channels = 0;  ///< Number of color channels per pixel
  PixelType pixel_type = PixelType::kUInt8;  ///< Pixel data type
  DataLayout layout =
      DataLayout::kChannelsLast;  ///< Memory layout of pixel data

  // Optional padding information for border tiles
  int expected_width = 0;      ///< Full tile width expected by the format
  int expected_height = 0;     ///< Full tile height expected by the format
  bool needs_padding = false;  ///< Whether this tile needs padding for output

 private:
  mutable TileData data_;  ///< Raw pixel data (lazy-loaded, type-erased)
  mutable std::function<TileData()> data_producer_;  ///< Lazy data producer
  /// Thread-safe materialization flag (shared_ptr makes Tile copyable)
  mutable std::shared_ptr<std::once_flag> materialized_flag_ =
      std::make_shared<std::once_flag>();

 public:
  /**
   * @brief Default constructor creates an empty tile.
   */
  Tile() = default;

  /**
   * @brief Constructs an eager tile with specified dimensions and allocates
   * data storage immediately.
   *
   * @param x X coordinate of the tile's top-left corner
   * @param y Y coordinate of the tile's top-left corner
   * @param width Width of the tile in pixels
   * @param height Height of the tile in pixels
   * @param channels Number of color channels per pixel
   * @param layout Memory layout of the pixel data (default: kChannelsLast)
   * @param pixel_type Pixel data type (default: kUInt8)
   */
  Tile(int x, int y, int width, int height, int channels,
       DataLayout layout = DataLayout::kChannelsLast,
       PixelType pixel_type = PixelType::kUInt8)
      : x(x),
        y(y),
        width(width),
        height(height),
        channels(channels),
        pixel_type(pixel_type),
        layout(layout) {
    size_t num_elements = static_cast<size_t>(width) *
                          static_cast<size_t>(height) *
                          static_cast<size_t>(channels);
    switch (pixel_type) {
      case PixelType::kUInt8:
        data_ = std::vector<uint8_t>(num_elements);
        break;
      case PixelType::kUInt16:
        data_ = std::vector<uint16_t>(num_elements);
        break;
      case PixelType::kFloat32:
        data_ = std::vector<float>(num_elements);
        break;
      default:
        throw std::invalid_argument("Unsupported pixel type");
    }
  }

  /**
   * @brief Constructs a lazy tile with a data producer function.
   *
   * The data will be produced only when GetData() is first called.
   *
   * @param x X coordinate of the tile's top-left corner
   * @param y Y coordinate of the tile's top-left corner
   * @param width Width of the tile in pixels
   * @param height Height of the tile in pixels
   * @param channels Number of color channels per pixel
   * @param producer Function that produces the pixel data when called
   * @param layout Memory layout of the pixel data (default: kChannelsLast)
   * @param pixel_type Pixel data type (default: kUInt8)
   */
  Tile(int x, int y, int width, int height, int channels,
       std::function<TileData()> producer,
       DataLayout layout = DataLayout::kChannelsLast,
       PixelType pixel_type = PixelType::kUInt8)
      : x(x),
        y(y),
        width(width),
        height(height),
        channels(channels),
        pixel_type(pixel_type),
        layout(layout),
        data_producer_(std::move(producer)) {}

  /**
   * @brief Constructs an eager tile with padding information.
   *
   * @param x X coordinate of the tile's top-left corner
   * @param y Y coordinate of the tile's top-left corner
   * @param width Actual width of the tile in pixels
   * @param height Actual height of the tile in pixels
   * @param channels Number of color channels per pixel
   * @param expected_width Expected full tile width
   * @param expected_height Expected full tile height
   * @param layout Memory layout of the pixel data (default: kChannelsLast)
   * @param pixel_type Pixel data type (default: kUInt8)
   */
  Tile(int x, int y, int width, int height, int channels, int expected_width,
       int expected_height, DataLayout layout = DataLayout::kChannelsLast,
       PixelType pixel_type = PixelType::kUInt8)
      : x(x),
        y(y),
        width(width),
        height(height),
        channels(channels),
        pixel_type(pixel_type),
        layout(layout),
        expected_width(expected_width),
        expected_height(expected_height),
        needs_padding(width < expected_width || height < expected_height) {
    size_t num_elements = static_cast<size_t>(width) *
                          static_cast<size_t>(height) *
                          static_cast<size_t>(channels);
    switch (pixel_type) {
      case PixelType::kUInt8:
        data_ = std::vector<uint8_t>(num_elements);
        break;
      case PixelType::kUInt16:
        data_ = std::vector<uint16_t>(num_elements);
        break;
      case PixelType::kFloat32:
        data_ = std::vector<float>(num_elements);
        break;
      default:
        throw std::invalid_argument("Unsupported pixel type");
    }
  }

  /**
   * @brief Gets the pixel type.
   *
   * @return PixelType of the tile data
   */
  PixelType GetPixelType() const { return pixel_type; }

  /**
   * @brief Gets the variant data, materializing it if necessary.
   *
   * For lazy tiles, this triggers data production on first call.
   * Thread-safe: uses std::call_once to ensure the producer runs exactly once,
   * even if multiple threads call this method concurrently. The once_flag is
   * shared via shared_ptr, so copied tiles share the same materialization
   * state.
   *
   * @return Reference to the variant tile data
   */
  const TileData& GetVariantData() const {
    std::call_once(*materialized_flag_, [this]() {
      if (data_producer_) {
        data_ = data_producer_();
        data_producer_ = nullptr;  // Clear producer after use to free memory
      }
    });
    return data_;
  }

  /**
   * @brief Gets mutable access to variant data, materializing if necessary.
   *
   * @return Mutable reference to the variant tile data
   */
  TileData& GetVariantDataMut() {
    // Use const version to materialize, then return mutable reference
    GetVariantData();
    return data_;
  }

  /**
   * @brief Moves the variant data out of the tile.
   *
   * @return TileData variant (moved)
   */
  TileData MoveVariantData() {
    GetVariantData();  // Materialize if needed
    return std::move(data_);
  }

  /**
   * @brief Gets typed access to pixel data, materializing if necessary.
   *
   * @tparam T The pixel type (uint8_t, uint16_t, or float)
   * @return Reference to the typed pixel data vector
   * @throw std::bad_variant_access if the requested type doesn't match
   */
  template <typename T>
  const std::vector<T>& GetDataAs() const {
    GetVariantData();  // Materialize if needed
    return std::get<std::vector<T>>(data_);
  }

  /**
   * @brief Gets mutable typed access to pixel data, materializing if necessary.
   *
   * @tparam T The pixel type (uint8_t, uint16_t, or float)
   * @return Mutable reference to the typed pixel data vector
   * @throw std::bad_variant_access if the requested type doesn't match
   */
  template <typename T>
  std::vector<T>& GetDataAsMut() {
    GetVariantData();  // Materialize if needed
    return std::get<std::vector<T>>(data_);
  }

  /**
   * @brief Gets the pixel data as uint8_t (backward compatible).
   *
   * For lazy tiles, this triggers data production on first call.
   * Subsequent calls return the cached data.
   *
   * @return Reference to the pixel data vector
   * @throw std::bad_variant_access if pixel type is not uint8
   */
  const std::vector<uint8_t>& GetData() const { return GetDataAs<uint8_t>(); }

  /**
   * @brief Gets mutable access to pixel data as uint8_t (backward compatible).
   *
   * @return Mutable reference to the pixel data vector
   * @throw std::bad_variant_access if pixel type is not uint8
   */
  std::vector<uint8_t>& GetDataMut() { return GetDataAsMut<uint8_t>(); }

  /**
   * @brief Gets raw pointer to pixel data (type-erased).
   *
   * This method returns a void pointer to the underlying data, useful for
   * generic operations that need to work with different pixel types.
   *
   * @return void* pointing to the pixel data
   */
  void* GetDataPtr() {
    GetVariantData();  // Materialize if needed
    return std::visit(
        [](auto& vec) -> void* { return static_cast<void*>(vec.data()); },
        data_);
  }

  /**
   * @brief Gets const raw pointer to pixel data (type-erased).
   *
   * @return const void* pointing to the pixel data
   */
  const void* GetDataPtr() const {
    GetVariantData();  // Materialize if needed
    return std::visit(
        [](const auto& vec) -> const void* {
          return static_cast<const void*>(vec.data());
        },
        data_);
  }

  /**
   * @brief Gets the size of the actual tile data in bytes.
   *
   * @return Size of the tile data in bytes
   */
  size_t GetDataSize() const {
    return static_cast<size_t>(width) * static_cast<size_t>(height) *
           static_cast<size_t>(channels) * GetPixelTypeSize(pixel_type);
  }

  /**
   * @brief Gets the expected full tile size in bytes.
   *
   * If padding information is available, returns the size for the full
   * expected tile dimensions. Otherwise, returns the actual data size.
   *
   * @return Expected full tile size in bytes
   */
  size_t GetExpectedDataSize() const {
    if (expected_width > 0 && expected_height > 0) {
      return static_cast<size_t>(expected_width) *
             static_cast<size_t>(expected_height) *
             static_cast<size_t>(channels) * GetPixelTypeSize(pixel_type);
    }
    return GetDataSize();
  }

  /**
   * @brief Creates zero-padded data for full tile size (type-erased).
   *
   * If padding is not needed, returns the original data as a variant.
   * Otherwise, creates a new vector with the expected dimensions and copies the
   * actual tile data to the top-left corner, filling the rest with zeros.
   *
   * Note: This materializes lazy tiles.
   *
   * @return TileData variant containing padded tile data
   */
  TileData CreatePaddedData() const {
    const auto& variant_data = GetVariantData();  // Materialize if needed

    if (!needs_padding) {
      return variant_data;
    }

    // Use visitor pattern to create padded data for the correct type
    return std::visit(
        [this](const auto& tile_data) -> TileData {
          using T = typename std::decay_t<decltype(tile_data)>::value_type;
          size_t padded_elements = static_cast<size_t>(expected_width) *
                                   static_cast<size_t>(expected_height) *
                                   static_cast<size_t>(channels);
          std::vector<T> padded_data(padded_elements, T(0));

          if (layout == DataLayout::kChannelsLast) {
            // Copy row by row instead of pixel by pixel for better performance
            // (HWC)
            for (int y = 0; y < height; ++y) {
              const T* src = tile_data.data() + y * width * channels;
              T* dst = padded_data.data() + y * expected_width * channels;
              std::memcpy(dst, src, width * channels * sizeof(T));
            }
            return padded_data;
          }

          // Channels-first (CHW): pad each plane row-by-row.
          for (int c = 0; c < channels; ++c) {
            for (int y = 0; y < height; ++y) {
              const T* src = tile_data.data() + (c * height + y) * width;
              T* dst = padded_data.data() +
                       (c * expected_height + y) * expected_width;
              std::memcpy(dst, src, width * sizeof(T));
            }
          }

          return padded_data;
        },
        variant_data);
  }

  /**
   * @brief Creates zero-padded data as bytes (backward compatible).
   *
   * This is a convenience method that returns the padded data as a byte vector,
   * regardless of the underlying pixel type. Useful for serialization and
   * compression.
   *
   * @return std::vector<uint8_t> containing padded tile data as raw bytes
   */
  std::vector<uint8_t> CreatePaddedDataAsBytes() const {
    TileData padded = CreatePaddedData();
    return std::visit(
        [](const auto& vec) -> std::vector<uint8_t> {
          const uint8_t* bytes = reinterpret_cast<const uint8_t*>(vec.data());
          size_t num_bytes =
              vec.size() *
              sizeof(typename std::decay_t<decltype(vec)>::value_type);
          return std::vector<uint8_t>(bytes, bytes + num_bytes);
        },
        padded);
  }

  /**
   * @brief Sets tile data from a typed vector.
   *
   * This templated method directly moves the typed data into the tile's variant
   * storage, avoiding unnecessary allocations and copies. It validates both the
   * buffer size and pixel type, throwing if there's a mismatch.
   *
   * @tparam T The pixel type (uint8_t, uint16_t, or float)
   * @param data Typed data to assign (moved)
   * @throw std::invalid_argument if data size doesn't match expected size or
   *        pixel type mismatch
   */
  template <typename T>
  void SetData(std::vector<T>&& data) {
    // Compile-time check for supported types
    static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                      std::is_same_v<T, float>,
                  "SetData only supports uint8_t, uint16_t, and float");

    // Compute expected pixel type from template parameter
    PixelType expected_type;
    if constexpr (std::is_same_v<T, uint8_t>) {
      expected_type = PixelType::kUInt8;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
      expected_type = PixelType::kUInt16;
    } else if constexpr (std::is_same_v<T, float>) {
      expected_type = PixelType::kFloat32;
    }

    // Validate buffer size
    size_t expected_elements = static_cast<size_t>(width) *
                               static_cast<size_t>(height) *
                               static_cast<size_t>(channels);
    if (data.size() != expected_elements) {
      std::string type_name;
      if constexpr (std::is_same_v<T, uint8_t>) {
        type_name = "kUInt8";
      } else if constexpr (std::is_same_v<T, uint16_t>) {
        type_name = "kUInt16";
      } else {
        type_name = "kFloat32";
      }
      throw std::invalid_argument(
          "Buffer size mismatch for " + type_name + ": expected " +
          std::to_string(expected_elements) + " elements, got " +
          std::to_string(data.size()) + " elements");
    }

    // Validate pixel type matches
    if (pixel_type != expected_type) {
      throw std::invalid_argument("Pixel type mismatch");
    }

    data_ = std::move(data);
    // Mark as materialized by calling the once_flag
    std::call_once(*materialized_flag_, []() {});
  }
};

/**
 * @brief Represents complete information about an image.
 *
 * This structure encapsulates the width, height, number of channels,
 * pixel type, and memory layout of an image, providing a convenient way to
 * pass around image metadata. Unlike ImageDimensions (deprecated), this
 * requires explicit pixel_type and layout to prevent silent type coercion.
 */
struct ImageInfo {
  Dimensions dimensions;  ///< Spatial dimensions (width and height)
  int channels = 0;       ///< Number of color channels per pixel
  PixelType pixel_type;   ///< Pixel data type (required, no default)
  DataLayout layout;  ///< Memory layout of pixel data (required, no default)

  /**
   * @brief Default constructor creates zero-dimensional image.
   *
   * Note: pixel_type and layout are uninitialized. This constructor exists
   * for compatibility but should not be used directly.
   */
  ImageInfo() = default;

  /**
   * @brief Constructs image info with specified values.
   *
   * All parameters are required to ensure explicit specification of
   * pixel type and layout.
   *
   * @param w Width of the image in pixels
   * @param h Height of the image in pixels
   * @param c Number of color channels per pixel
   * @param p Pixel data type
   * @param l Memory layout of the pixel data
   */
  ImageInfo(int w, int h, int c, PixelType p, DataLayout l)
      : dimensions(w, h), channels(c), pixel_type(p), layout(l) {}

  /**
   * @brief Gets the width of the image.
   *
   * Convenience accessor for dimensions.width.
   *
   * @return Width of the image in pixels
   */
  int GetWidth() const { return dimensions.width; }

  /**
   * @brief Gets the height of the image.
   *
   * Convenience accessor for dimensions.height.
   *
   * @return Height of the image in pixels
   */
  int GetHeight() const { return dimensions.height; }
};

/**
 * @brief Represents the preferred tile size for processing.
 *
 * This structure specifies the optimal tile dimensions for efficient
 * processing of an image source. Different image formats may have
 * different optimal tile sizes based on their internal structure.
 */
struct TileSize {
  int width = 0;   ///< Preferred tile width in pixels
  int height = 0;  ///< Preferred tile height in pixels

  /**
   * @brief Default constructor creates zero-sized tile.
   */
  TileSize() = default;

  /**
   * @brief Constructs tile size with specified dimensions.
   *
   * @param w Preferred tile width in pixels
   * @param h Preferred tile height in pixels
   */
  TileSize(int w, int h) : width(w), height(h) {}
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_TYPES_H_

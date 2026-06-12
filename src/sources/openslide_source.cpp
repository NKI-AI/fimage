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

#include "fim/sources/openslide_source.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace fim {

namespace {

// Optimized helper function to convert OpenSlide ARGB to RGBA in-place
// OpenSlide returns pre-multiplied ARGB in platform byte order
// Based on the optimized implementation from openslide_go
void ConvertARGBToRGBA(uint32_t* buf, size_t pixel_count) {
  for (size_t i = 0; i < pixel_count; ++i) {
    uint32_t pixel = buf[i];

    uint8_t a = (pixel >> 24) & 0xFF;
    uint8_t r = (pixel >> 16) & 0xFF;
    uint8_t g = (pixel >> 8) & 0xFF;
    uint8_t b = (pixel >> 0) & 0xFF;

    // Un-premultiply if alpha is not fully opaque or transparent
    if (a != 0 && a != 255) {
      r = static_cast<uint8_t>((255 * r) / a);
      g = static_cast<uint8_t>((255 * g) / a);
      b = static_cast<uint8_t>((255 * b) / a);
    }

    // Repack as RGBA (platform byte order)
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    buf[i] = (r << 24) | (g << 16) | (b << 8) | a;
#else
    buf[i] = (a << 24) | (b << 16) | (g << 8) | r;
#endif
  }
}

// Helper function to convert RGBA to RGB by dropping alpha channel
void RGBAToRGB(const uint32_t* rgba_data, uint8_t* rgb_data,
               size_t pixel_count) {
  const uint8_t* rgba_bytes = reinterpret_cast<const uint8_t*>(rgba_data);
  for (size_t i = 0; i < pixel_count; ++i) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    rgb_data[i * 3 + 0] = rgba_bytes[i * 4 + 0];  // R
    rgb_data[i * 3 + 1] = rgba_bytes[i * 4 + 1];  // G
    rgb_data[i * 3 + 2] = rgba_bytes[i * 4 + 2];  // B
    // Drop A (rgba_bytes[i * 4 + 3])
#else
    rgb_data[i * 3 + 0] = rgba_bytes[i * 4 + 0];  // R
    rgb_data[i * 3 + 1] = rgba_bytes[i * 4 + 1];  // G
    rgb_data[i * 3 + 2] =
        rgba_bytes[i * 4 + 2];  // B
                                // Drop A (rgba_bytes[i * 4 + 3])
#endif
  }
}

// Convert level-specific coordinates to level 0 coordinates
void ConvertLevelToLevel0(int level_x, int level_y, double downsample,
                          int64_t& level0_x, int64_t& level0_y) {
  level0_x = static_cast<int64_t>(std::round(level_x * downsample));
  level0_y = static_cast<int64_t>(std::round(level_y * downsample));
}

}  // namespace

// ============================================================================
// OpenSlideLevelView Implementation
// ============================================================================

OpenSlideLevelView::OpenSlideLevelView(std::shared_ptr<openslide_t> handle,
                                       std::shared_ptr<std::mutex> mutex,
                                       int level, Dimensions dimensions,
                                       double downsample, int channels,
                                       DataLayout memory_format, bool rgb)
    : handle_(std::move(handle)),
      mutex_(std::move(mutex)),
      level_(level),
      dimensions_(dimensions),
      downsample_(downsample),
      channels_(channels),
      memory_format_(memory_format),
      rgb_(rgb) {
  if (!handle_) {
    throw std::invalid_argument("OpenSlide handle cannot be null");
  }
  if (!mutex_) {
    throw std::invalid_argument("Mutex cannot be null");
  }
  if (level < 0) {
    throw std::out_of_range("Invalid pyramid level: " + std::to_string(level));
  }
}

ImageInfo OpenSlideLevelView::GetDimensions() const {
  return ImageInfo(dimensions_.width, dimensions_.height, channels_,
                   PixelType::kUInt8, memory_format_);
}

TileSize OpenSlideLevelView::GetIdealTileSize() const {
  // OpenSlide doesn't expose native tile sizes, use a reasonable default
  return TileSize(512, 512);
}

Tile OpenSlideLevelView::GetTile(int x, int y, int width, int height,
                                 bool rgb) const {
  // Convert level coordinates to level 0 coordinates
  int64_t level0_x, level0_y;
  ConvertLevelToLevel0(x, y, downsample_, level0_x, level0_y);

  // Clamp to valid coordinates
  x = std::max(0, x);
  y = std::max(0, y);
  width = std::max(0, width);
  height = std::max(0, height);

  // Allocate buffer for ARGB data from OpenSlide
  std::vector<uint32_t> argb_buffer(width * height);

  // Read region from OpenSlide (thread-safe)
  {
    std::lock_guard<std::mutex> lock(*mutex_);
    openslide_read_region(handle_.get(), argb_buffer.data(), level0_x, level0_y,
                          level_, width, height);

    // Check for errors
    const char* error = openslide_get_error(handle_.get());
    if (error != nullptr) {
      throw std::runtime_error("Failed to read region from slide: " +
                               std::string(error));
    }
  }

  // Convert ARGB to RGBA in-place (optimized)
  ConvertARGBToRGBA(argb_buffer.data(), width * height);

  if (rgb) {
    // Create tile and convert RGBA to RGB (dropping alpha)
    Tile tile(x, y, width, height, 3);
    auto& dest = tile.GetDataMut();
    RGBAToRGB(argb_buffer.data(), dest.data(), width * height);
    return tile;
  } else {
    // Return RGBA data directly
    Tile tile(x, y, width, height, 4);
    auto& dest = tile.GetDataMut();
    std::memcpy(dest.data(), argb_buffer.data(), width * height * 4);
    return tile;
  }
}

// ============================================================================
// OpenSlideSource Implementation
// ============================================================================

OpenSlideSource::OpenSlideSource(const fs::path& filename)
    : mutex_(std::make_shared<std::mutex>()), filename_(filename) {
  // Open slide using OpenSlide
  openslide_t* raw_handle = openslide_open(filename.string().c_str());

  if (raw_handle == nullptr) {
    throw std::runtime_error("Failed to open slide '" + filename.string() +
                             "': OpenSlide returned null");
  }

  // Check for errors after opening
  const char* error = openslide_get_error(raw_handle);
  if (error != nullptr) {
    openslide_close(raw_handle);
    throw std::runtime_error("Failed to open slide '" + filename.string() +
                             "': " + std::string(error));
  }

  // Validate that the slide has at least one level
  int32_t level_count = openslide_get_level_count(raw_handle);
  if (level_count <= 0) {
    openslide_close(raw_handle);
    throw std::runtime_error("Slide has no pyramid levels");
  }

  // Wrap in shared_ptr with custom deleter
  handle_ = std::shared_ptr<openslide_t>(raw_handle, [](openslide_t* handle) {
    if (handle != nullptr) {
      openslide_close(handle);
    }
  });
}

OpenSlideSource OpenSlideSource::Create(const fs::path& filename) {
  return OpenSlideSource(filename);
}

OpenSlideLevelView OpenSlideSource::LevelView(int level, bool rgb) const {
  if (level < 0 || level >= GetLevelCount()) {
    throw std::out_of_range("Invalid pyramid level: " + std::to_string(level) +
                            " (valid range: 0-" +
                            std::to_string(GetLevelCount() - 1) + ")");
  }
  // Get level metadata to cache in the view
  Dimensions dims = GetLevelDimensions(level);
  double downsample = GetLevelDownsample(level);
  int channels = GetNumChannels(rgb);
  DataLayout layout = GetMemoryFormat();

  // Create view with shared ownership of handle/mutex and cached metadata
  return OpenSlideLevelView(handle_, mutex_, level, dims, downsample, channels,
                            layout, rgb);
}

int OpenSlideSource::GetLevelCount() const {
  std::lock_guard<std::mutex> lock(*mutex_);
  return openslide_get_level_count(handle_.get());
}

Dimensions OpenSlideSource::GetLevelDimensions(int level) const {
  if (level < 0 || level >= GetLevelCount()) {
    throw std::out_of_range("Invalid pyramid level: " + std::to_string(level));
  }

  int64_t width, height;
  {
    std::lock_guard<std::mutex> lock(*mutex_);
    openslide_get_level_dimensions(handle_.get(), level, &width, &height);

    // Check for errors
    const char* error = openslide_get_error(handle_.get());
    if (error != nullptr) {
      throw std::runtime_error("Failed to get level dimensions for level " +
                               std::to_string(level) + ": " +
                               std::string(error));
    }
  }

  return Dimensions(static_cast<int>(width), static_cast<int>(height));
}

int OpenSlideSource::GetNumChannels(bool rgb) const {
  // Return 3 for RGB or 4 for RGBA
  return rgb ? 3 : 4;
}

DataLayout OpenSlideSource::GetMemoryFormat() const {
  // We convert to channels-last (RGB) format
  return DataLayout::kChannelsLast;
}

double OpenSlideSource::GetLevelDownsample(int level) const {
  if (level < 0 || level >= GetLevelCount()) {
    throw std::out_of_range("Invalid pyramid level: " + std::to_string(level));
  }

  std::lock_guard<std::mutex> lock(*mutex_);
  double downsample = openslide_get_level_downsample(handle_.get(), level);

  // Check for errors
  const char* error = openslide_get_error(handle_.get());
  if (error != nullptr) {
    throw std::runtime_error("Failed to get level downsample for level " +
                             std::to_string(level) + ": " + std::string(error));
  }

  return downsample;
}

std::array<double, 2> OpenSlideSource::GetMpp() const {
  std::lock_guard<std::mutex> lock(*mutex_);

  // Try to get MPP from properties
  const char* mpp_x_str = openslide_get_property_value(
      handle_.get(), OPENSLIDE_PROPERTY_NAME_MPP_X);
  const char* mpp_y_str = openslide_get_property_value(
      handle_.get(), OPENSLIDE_PROPERTY_NAME_MPP_Y);

  double mpp_x = 0.0;
  double mpp_y = 0.0;

  if (mpp_x_str != nullptr) {
    try {
      mpp_x = std::stod(mpp_x_str);
    } catch (...) {
      // Ignore conversion errors, leave as 0.0
    }
  }

  if (mpp_y_str != nullptr) {
    try {
      mpp_y = std::stod(mpp_y_str);
    } catch (...) {
      // Ignore conversion errors, leave as 0.0
    }
  }

  return {mpp_x, mpp_y};
}

std::string OpenSlideSource::GetFormatName() const {
  std::lock_guard<std::mutex> lock(*mutex_);

  const char* vendor = openslide_get_property_value(
      handle_.get(), OPENSLIDE_PROPERTY_NAME_VENDOR);

  if (vendor != nullptr) {
    return std::string(vendor);
  }

  return "unknown";
}

std::array<int64_t, 4> OpenSlideSource::GetBounds() const {
  std::lock_guard<std::mutex> lock(*mutex_);

  // Try to get bounds properties
  const char* bounds_x_str = openslide_get_property_value(
      handle_.get(), OPENSLIDE_PROPERTY_NAME_BOUNDS_X);
  const char* bounds_y_str = openslide_get_property_value(
      handle_.get(), OPENSLIDE_PROPERTY_NAME_BOUNDS_Y);
  const char* bounds_width_str = openslide_get_property_value(
      handle_.get(), OPENSLIDE_PROPERTY_NAME_BOUNDS_WIDTH);
  const char* bounds_height_str = openslide_get_property_value(
      handle_.get(), OPENSLIDE_PROPERTY_NAME_BOUNDS_HEIGHT);

  // All four properties must be present for valid bounds
  if (bounds_x_str == nullptr || bounds_y_str == nullptr ||
      bounds_width_str == nullptr || bounds_height_str == nullptr) {
    // Return default bounds: (0, 0, width, height) from level 0
    int64_t width, height;
    openslide_get_level_dimensions(handle_.get(), 0, &width, &height);
    return std::array<int64_t, 4>{0, 0, width, height};
  }

  try {
    int64_t x = std::stoll(bounds_x_str);
    int64_t y = std::stoll(bounds_y_str);
    int64_t width = std::stoll(bounds_width_str);
    int64_t height = std::stoll(bounds_height_str);
    return std::array<int64_t, 4>{x, y, width, height};
  } catch (...) {
    // Conversion failed, return default bounds from level 0
    int64_t width, height;
    openslide_get_level_dimensions(handle_.get(), 0, &width, &height);
    return std::array<int64_t, 4>{0, 0, width, height};
  }
}

std::map<std::string, std::string> OpenSlideSource::GetAllProperties() const {
  std::map<std::string, std::string> properties;

  std::lock_guard<std::mutex> lock(*mutex_);

  // Get all property names
  const char* const* names = openslide_get_property_names(handle_.get());
  if (names == nullptr) {
    return properties;
  }

  // Iterate through all properties
  for (int i = 0; names[i] != nullptr; ++i) {
    const char* key = names[i];
    const char* value = openslide_get_property_value(handle_.get(), key);
    if (value != nullptr) {
      properties[key] = value;
    }
  }

  return properties;
}

std::vector<std::string> OpenSlideSource::GetAssociatedImageNames() const {
  std::vector<std::string> names;

  std::lock_guard<std::mutex> lock(*mutex_);

  // Get all associated image names
  const char* const* image_names =
      openslide_get_associated_image_names(handle_.get());
  if (image_names == nullptr) {
    return names;
  }

  // Iterate through all names
  for (int i = 0; image_names[i] != nullptr; ++i) {
    names.emplace_back(image_names[i]);
  }

  return names;
}

std::tuple<std::vector<uint8_t>, int, int> OpenSlideSource::ReadAssociatedImage(
    const std::string& name) const {
  int64_t width, height;

  // Get dimensions first
  {
    std::lock_guard<std::mutex> lock(*mutex_);
    openslide_get_associated_image_dimensions(handle_.get(), name.c_str(),
                                              &width, &height);

    const char* error = openslide_get_error(handle_.get());
    if (error != nullptr) {
      throw std::runtime_error(
          "Failed to get dimensions for associated image '" + name +
          "': " + std::string(error));
    }

    if (width <= 0 || height <= 0) {
      throw std::runtime_error("Invalid dimensions for associated image '" +
                               name + "'");
    }
  }

  // Allocate buffer for ARGB data
  std::vector<uint32_t> argb_buffer(width * height);

  // Read the associated image
  {
    std::lock_guard<std::mutex> lock(*mutex_);
    openslide_read_associated_image(handle_.get(), name.c_str(),
                                    argb_buffer.data());

    const char* error = openslide_get_error(handle_.get());
    if (error != nullptr) {
      throw std::runtime_error("Failed to read associated image '" + name +
                               "': " + std::string(error));
    }
  }

  // Convert ARGB to RGB
  ConvertARGBToRGBA(argb_buffer.data(), width * height);

  // Drop alpha channel
  std::vector<uint8_t> rgb_data(width * height * 3);
  RGBAToRGB(argb_buffer.data(), rgb_data.data(), width * height);

  return {std::move(rgb_data), static_cast<int>(width),
          static_cast<int>(height)};
}

}  // namespace fim

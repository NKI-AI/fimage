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

#include "fim/sources/fastslide_source.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>

#include "fastslide/core/slide_descriptor.h"
#include "fastslide/image.h"
#include "fastslide/readers/mrxs/mrxs.h"
#include "fastslide/runtime/reader_registry.h"
#include "fim/utilities/debug_flags.h"

namespace fim {

namespace {

PixelType FastSlideDataTypeToPixelType(fastslide::DataType dtype) {
  switch (dtype) {
    case fastslide::DataType::kUInt8:
      return PixelType::kUInt8;
    case fastslide::DataType::kUInt16:
      return PixelType::kUInt16;
    case fastslide::DataType::kFloat32:
      return PixelType::kFloat32;
    default:
      throw std::runtime_error("Unsupported fastslide dtype for fimage: " +
                               std::string(fastslide::GetName(dtype)));
  }
}

// Helper function to convert fastslide::Image to fim::Tile
Tile ConvertFastSlideImageToTile(const fastslide::Image& fs_image, int x,
                                 int y) {
  const uint32_t width = fs_image.GetDimensions()[0];
  const uint32_t height = fs_image.GetDimensions()[1];
  const uint32_t channels = fs_image.GetChannels();

  const fastslide::DataType dtype = fs_image.GetDataType();
  const fastslide::PlanarConfig planar = fs_image.GetPlanarConfig();
  const uint8_t* src_data = fs_image.GetData();
  const size_t pixel_count = width * height;

  const DataLayout layout = (planar == fastslide::PlanarConfig::kContiguous)
                                ? DataLayout::kChannelsLast
                                : DataLayout::kChannelsFirst;

  const PixelType pixel_type = FastSlideDataTypeToPixelType(dtype);

  Tile tile(x, y, static_cast<int>(width), static_cast<int>(height),
            static_cast<int>(channels), layout, pixel_type);

  // Copy raw bytes. fastslide::PlanarConfig maps directly to
  // fimage::DataLayout. The total element count is width * height * channels
  // regardless of layout.
  fastslide::DispatchByDataType(dtype, [&]<typename T>() {
    if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                  std::is_same_v<T, float>) {
      auto& dest = tile.GetDataAsMut<T>();
      const size_t elem_count = pixel_count * channels;
      if (dest.size() != elem_count) {
        throw std::runtime_error("Tile buffer size mismatch");
      }
      std::memcpy(dest.data(), src_data, elem_count * sizeof(T));
    } else {
      throw std::runtime_error("Unsupported fastslide dtype for fimage: " +
                               std::string(fastslide::GetName(dtype)));
    }
  });

  return tile;
}

}  // namespace

// ============================================================================
// FastSlideLevelView Implementation
// ============================================================================

FastSlideLevelView::FastSlideLevelView(
    std::shared_ptr<const fastslide::SlideReader> reader, int level,
    Dimensions dimensions, int channels, PixelType pixel_type,
    DataLayout memory_format)
    : reader_(std::move(reader)),
      level_(level),
      dimensions_(dimensions),
      channels_(channels),
      pixel_type_(pixel_type),
      memory_format_(memory_format) {
  if (!reader_) {
    throw std::invalid_argument("SlideReader pointer cannot be null");
  }
  if (level < 0 || level >= reader_->GetLevelCount()) {
    throw std::out_of_range("Invalid pyramid level: " + std::to_string(level));
  }
}

ImageInfo FastSlideLevelView::GetDimensions() const {
  return ImageInfo(dimensions_.width, dimensions_.height, channels_,
                   pixel_type_, memory_format_);
}

TileSize FastSlideLevelView::GetIdealTileSize() const {
  const auto tile_dims = reader_->GetTileSize();
  return TileSize(static_cast<int>(tile_dims[0]),
                  static_cast<int>(tile_dims[1]));
}

Tile FastSlideLevelView::GetTile(int x, int y, int width, int height) const {
  const bool debug = debug_flags::IsEnabled("FIM_DEBUG_TO_NUMPY") ||
                     debug_flags::IsEnabled("FIM_DEBUG_FASTSLIDE");
  const bool serialize_reads =
      debug_flags::IsEnabled("FIM_FASTSLIDE_SERIALIZE_READS");
  static std::mutex* global_mutex = []() -> std::mutex* {
    // Leak on purpose: avoids shutdown-order issues.
    return new std::mutex();
  }();
  const auto t0 = std::chrono::steady_clock::now();
  if (debug) {
    std::fprintf(stderr,
                 "[fim] FastSlideLevelView::GetTile(level=%d) x=%d y=%d w=%d "
                 "h=%d serialize_reads=%d\n",
                 level_, x, y, width, height, serialize_reads ? 1 : 0);
    std::fflush(stderr);
  }

  // Create RegionSpec in the selected level's coordinate space
  fastslide::RegionSpec region;
  region.level = level_;
  region.top_left[0] = static_cast<uint32_t>(std::max(0, x));
  region.top_left[1] = static_cast<uint32_t>(std::max(0, y));
  region.size[0] = static_cast<uint32_t>(std::max(0, width));
  region.size[1] = static_cast<uint32_t>(std::max(0, height));

  // Read region from fastslide
  std::unique_lock<std::mutex> lock(*global_mutex, std::defer_lock);
  if (serialize_reads) {
    lock.lock();
  }
  auto image_or = reader_->ReadRegion(region);
  if (!image_or.ok()) {
    throw std::runtime_error("Failed to read region from slide: " +
                             std::string(image_or.status().message()));
  }

  // Convert fastslide::Image to fim::Tile
  Tile out = ConvertFastSlideImageToTile(image_or.value(), x, y);
  if (debug) {
    const auto t1 = std::chrono::steady_clock::now();
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::fprintf(stderr, "[fim] FastSlideLevelView::GetTile: done in %lld ms\n",
                 static_cast<long long>(ms));
    std::fflush(stderr);
  }
  return out;
}

// ============================================================================
// FastSlideSource Implementation
// ============================================================================

FastSlideSource::FastSlideSource(const fs::path& filename)
    : filename_(filename) {
  // Open slide using fastslide's global registry
  auto reader_or =
      fastslide::runtime::GetGlobalRegistry().CreateReader(filename.string());

  if (!reader_or.ok()) {
    throw std::runtime_error("Failed to open slide '" + filename.string() +
                             "': " + std::string(reader_or.status().message()));
  }

  reader_ = std::move(reader_or.value());

  // Validate that the slide has at least one level
  if (reader_->GetLevelCount() == 0) {
    throw std::runtime_error("Slide has no pyramid levels");
  }

  // Detect the output planar configuration used by this reader so that
  // GetDimensions() reports the correct DataLayout.
  // We do this once up-front to keep the layout constant for the lifetime of
  // the source.
  fastslide::RegionSpec probe;
  probe.level = 0;
  probe.top_left[0] = 0;
  probe.top_left[1] = 0;
  probe.size[0] = 1;
  probe.size[1] = 1;
  auto image_or = reader_->ReadRegion(probe);
  if (image_or.ok()) {
    memory_format_ =
        (image_or->GetPlanarConfig() == fastslide::PlanarConfig::kContiguous
             ? DataLayout::kChannelsLast
             : DataLayout::kChannelsFirst);
    pixel_type_ = FastSlideDataTypeToPixelType(image_or->GetDataType());
  }
}

FastSlideSource FastSlideSource::Create(const fs::path& filename) {
  return FastSlideSource(filename);
}

FastSlideLevelView FastSlideSource::LevelView(int level) const {
  if (level < 0 || level >= GetLevelCount()) {
    throw std::out_of_range("Invalid pyramid level: " + std::to_string(level) +
                            " (valid range: 0-" +
                            std::to_string(GetLevelCount() - 1) + ")");
  }
  // Get level dimensions to cache in the view
  Dimensions dims = GetLevelDimensions(level);
  int channels = GetNumChannels();
  DataLayout layout = GetMemoryFormat();
  PixelType pixel_type = GetPixelType();

  // Create view with shared ownership of reader and cached metadata
  return FastSlideLevelView(reader_, level, dims, channels, pixel_type, layout);
}

int FastSlideSource::GetLevelCount() const {
  return reader_->GetLevelCount();
}

Dimensions FastSlideSource::GetLevelDimensions(int level) const {
  if (level < 0 || level >= GetLevelCount()) {
    throw std::out_of_range("Invalid pyramid level: " + std::to_string(level));
  }

  auto level_info_or = reader_->GetLevelInfo(level);
  if (!level_info_or.ok()) {
    throw std::runtime_error("Failed to get level info for level " +
                             std::to_string(level) + ": " +
                             std::string(level_info_or.status().message()));
  }

  const auto& dims = level_info_or.value().dimensions;
  return Dimensions(static_cast<int>(dims[0]), static_cast<int>(dims[1]));
}

int FastSlideSource::GetNumChannels() const {
  // Get channel count from the image format
  // Most WSI formats are RGB (3 channels)
  int channels = 3;
  const auto format = reader_->GetImageFormat();
  if (format == fastslide::ImageFormat::kRGBA) {
    channels = 4;
  } else if (format == fastslide::ImageFormat::kGray) {
    channels = 1;
  } else if (format == fastslide::ImageFormat::kSpectral) {
    // For spectral images, get channel count from metadata
    auto channel_metadata = reader_->GetChannelMetadata();
    channels = static_cast<int>(channel_metadata.size());
  }
  return channels;
}

DataLayout FastSlideSource::GetMemoryFormat() const {
  return memory_format_;
}

double FastSlideSource::GetLevelDownsample(int level) const {
  if (level < 0 || level >= GetLevelCount()) {
    throw std::out_of_range("Invalid pyramid level: " + std::to_string(level));
  }

  auto level_info_or = reader_->GetLevelInfo(level);
  if (!level_info_or.ok()) {
    throw std::runtime_error("Failed to get level info for level " +
                             std::to_string(level) + ": " +
                             std::string(level_info_or.status().message()));
  }

  return level_info_or.value().downsample_factor;
}

std::array<double, 2> FastSlideSource::GetMpp() const {
  const auto& props = reader_->GetProperties();
  return {props.mpp[0], props.mpp[1]};
}

std::string FastSlideSource::GetFormatName() const {
  return reader_->GetFormatName();
}

std::array<int64_t, 4> FastSlideSource::GetBounds() const {
  const auto& props = reader_->GetProperties();
  if (props.bounds.IsValid()) {
    return std::array<int64_t, 4>{props.bounds.x, props.bounds.y,
                                  props.bounds.width, props.bounds.height};
  }
  // Return default bounds: (0, 0, width, height) from level 0
  auto dims = GetLevelDimensions(0);
  return std::array<int64_t, 4>{0, 0, dims.width, dims.height};
}

std::vector<fastslide::ChannelMetadata> FastSlideSource::GetChannelMetadata()
    const {
  return reader_->GetChannelMetadata();
}

std::map<std::string, std::string> FastSlideSource::GetAllProperties() const {
  std::map<std::string, std::string> properties;
  const auto& props = reader_->GetProperties();

  // Add physical properties
  if (props.mpp[0] > 0.0) {
    properties["mpp_x"] = std::to_string(props.mpp[0]);
  }
  if (props.mpp[1] > 0.0) {
    properties["mpp_y"] = std::to_string(props.mpp[1]);
  }
  if (props.objective_magnification > 0.0) {
    properties["objective_magnification"] =
        std::to_string(props.objective_magnification);
  }
  if (!props.objective_name.empty()) {
    properties["objective_name"] = props.objective_name;
  }
  if (!props.scanner_model.empty()) {
    properties["scanner_model"] = props.scanner_model;
  }
  if (props.scan_date.has_value()) {
    properties["scan_date"] = *props.scan_date;
  }

  // Add bounds if valid
  if (props.bounds.IsValid()) {
    properties["bounds_x"] = std::to_string(props.bounds.x);
    properties["bounds_y"] = std::to_string(props.bounds.y);
    properties["bounds_width"] = std::to_string(props.bounds.width);
    properties["bounds_height"] = std::to_string(props.bounds.height);
  }

  // Add format name
  properties["format"] = reader_->GetFormatName();

  // Add level count
  properties["level_count"] = std::to_string(reader_->GetLevelCount());

  return properties;
}

std::vector<std::string> FastSlideSource::GetAssociatedImageNames() const {
  std::vector<std::string> names = reader_->GetAssociatedImageNames();

  // Also add image-type data from MRXS nonhier layers
  auto* mrxs_reader = dynamic_cast<fastslide::MrxsReader*>(reader_.get());
  if (mrxs_reader) {
    auto all_data_names = mrxs_reader->GetAssociatedDataNames();
    for (const auto& name : all_data_names) {
      // Move items starting with "ScanDataLayer_Slide" to associated_images
      const std::string prefix = "ScanDataLayer_Slide";
      if (name.find(prefix) == 0) {
        // Strip prefix and add to images
        std::string stripped_name = name.substr(prefix.length());
        names.push_back(stripped_name);
      }
    }
  }

  return names;
}

std::tuple<std::vector<uint8_t>, int, int> FastSlideSource::ReadAssociatedImage(
    const std::string& name) const {
  // For MRXS files, try nonhier data first since that's where associated images
  // are
  auto* mrxs_reader = dynamic_cast<fastslide::MrxsReader*>(reader_.get());
  if (mrxs_reader) {
    std::string full_name = "ScanDataLayer_Slide" + name;
    auto data_or = mrxs_reader->LoadAssociatedData(full_name);
    if (data_or.ok() && data_or->IsImage()) {
      const auto* image = data_or->GetImage();
      if (image) {
        const auto& dims = image->GetDimensions();
        const uint32_t width = dims[0];
        const uint32_t height = dims[1];
        const uint8_t* src_data = image->GetData();

        // MRXS associated images are RGB
        std::vector<uint8_t> rgb_data(src_data, src_data + width * height * 3);
        return {std::move(rgb_data), static_cast<int>(width),
                static_cast<int>(height)};
      }
    }
  }

  // Fall back to standard associated image method
  auto image_or = reader_->ReadAssociatedImage(name);
  if (!image_or.ok()) {
    throw std::runtime_error("Failed to read associated image '" + name +
                             "': " + std::string(image_or.status().message()));
  }

  const auto& image = image_or.value();
  const uint32_t width = image.GetDimensions()[0];
  const uint32_t height = image.GetDimensions()[1];
  const uint32_t channels = image.GetChannels();

  // Convert to RGB if necessary
  std::vector<uint8_t> rgb_data;
  const uint8_t* src_data = image.GetData();
  const size_t pixel_count = width * height;

  if (channels == 3) {
    // Already RGB, direct copy
    rgb_data.assign(src_data, src_data + pixel_count * 3);
  } else if (channels == 4) {
    // RGBA to RGB, drop alpha
    rgb_data.resize(pixel_count * 3);
    for (size_t i = 0; i < pixel_count; ++i) {
      rgb_data[i * 3 + 0] = src_data[i * 4 + 0];  // R
      rgb_data[i * 3 + 1] = src_data[i * 4 + 1];  // G
      rgb_data[i * 3 + 2] = src_data[i * 4 + 2];  // B
    }
  } else {
    throw std::runtime_error(
        "Unsupported channel count for associated image: " +
        std::to_string(channels));
  }

  return {std::move(rgb_data), static_cast<int>(width),
          static_cast<int>(height)};
}

}  // namespace fim

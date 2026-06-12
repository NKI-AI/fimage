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
 * @file tiff_sink.cpp
 * @brief Implementation of TIFF image sink for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the implementation of the TiffSink class which provides
 * TIFF image writing capabilities using libtiff. It handles assembling
 * complete images from tiles and writing them to TIFF files with appropriate
 * metadata and compression settings. It supports both single-page and
 * pyramidal TIFF output.
 */
#include "fim/sinks/tiff_sink.h"

#include <tiffio.h>

#include <string>

namespace fim {

TiffSink TiffSink::Create(const fs::path& filename) {
  return TiffSink(filename);
}

TiffSink TiffSink::Create(const fs::path& filename, const TileSize& tile_size) {
  return TiffSink(filename, tile_size);
}

TiffSink TiffSink::Create(const fs::path& filename, const TileSize& tile_size,
                          bool pyramidal, size_t memory_threshold_mb,
                          int downsample_factor) {
  return TiffSink(filename, tile_size, pyramidal, memory_threshold_mb,
                  downsample_factor);
}

TiffSink::TiffSink(const fs::path& filename)
    : SinkBase<TiffSink>(filename),
      tile_size_(256, 256),
      pyramidal_(false),
      memory_threshold_bytes_(0),  // Not used for non-pyramidal
      downsample_factor_(0) {}     // Not used for non-pyramidal

TiffSink::TiffSink(const fs::path& filename, const TileSize& tile_size)
    : SinkBase<TiffSink>(filename),
      tile_size_(tile_size),
      pyramidal_(false),
      memory_threshold_bytes_(0),  // Not used for non-pyramidal
      downsample_factor_(0) {}     // Not used for non-pyramidal

TiffSink::TiffSink(const fs::path& filename, const TileSize& tile_size,
                   bool pyramidal, size_t memory_threshold_mb,
                   int downsample_factor)
    : SinkBase<TiffSink>(filename),
      tile_size_(tile_size),
      pyramidal_(pyramidal),
      memory_threshold_bytes_(memory_threshold_mb * 1024 * 1024),
      downsample_factor_(downsample_factor) {}

void TiffSink::ConfigureTiffTags(TIFF* tiff, const ImageInfo& dims,
                                 int level_index) {
  TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, dims.GetWidth());
  TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, dims.GetHeight());
  TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, dims.channels);

  // Set bits per sample and sample format based on pixel type
  switch (dims.pixel_type) {
    case PixelType::kUInt8:
      TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
      TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
      break;
    case PixelType::kUInt16:
      TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 16);
      TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
      break;
    case PixelType::kFloat32:
      TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 32);
      TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
      break;
    default:
      throw std::runtime_error("Unsupported pixel type for TIFF output");
  }

  TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);

  // Set photometric interpretation based on channels
  if (dims.channels == 1) {
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
  } else if (dims.channels == 3) {
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  } else if (dims.channels == 4) {
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    uint16_t extra_samples = EXTRASAMPLE_ASSOCALPHA;
    TIFFSetField(tiff, TIFFTAG_EXTRASAMPLES, 1, &extra_samples);
  }

  // Set subfile type for pyramid levels (only for pyramidal TIFFs)
  if (pyramidal_ && level_index > 0) {
    TIFFSetField(tiff, TIFFTAG_SUBFILETYPE, FILETYPE_REDUCEDIMAGE);
  }

  // Set description indicating pyramid level (only for pyramidal TIFFs)
  if (pyramidal_) {
    std::string description = "Pyramid level " + std::to_string(level_index);
    if (level_index == 0) {
      description += " (full resolution)";
    } else {
      int reduction_factor = 1;
      for (int i = 0; i < level_index; ++i) {
        reduction_factor *= downsample_factor_;
      }
      description += " (1:" + std::to_string(reduction_factor) + " reduction)";
    }
    TIFFSetField(tiff, TIFFTAG_IMAGEDESCRIPTION, description.c_str());
  }
}

size_t TiffSink::CalculateMemorySize(const ImageInfo& dims) {
  return static_cast<size_t>(dims.GetWidth()) *
         static_cast<size_t>(dims.GetHeight()) *
         static_cast<size_t>(dims.channels) * GetPixelTypeSize(dims.pixel_type);
}

bool TiffSink::ShouldContinueDownsampling(const ImageInfo& dims) const {
  // Stop when either dimension becomes smaller than the tile size
  return dims.GetWidth() >= tile_size_.width &&
         dims.GetHeight() >= tile_size_.height;
}

}  // namespace fim

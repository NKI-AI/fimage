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
 * @file fimage_sink.cpp
 * @brief Implementation of FImage native format sink.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the implementation of the FImageSink class and related
 * structures for writing the custom FImage format with support for tiling,
 * compression, and metadata preservation.
 */
#include "fim/sinks/fimage_sink.h"

#include <cstdint>
#include <cstring>
#include <iostream>

#include "fim/utilities/compressor.h"

namespace fim {

namespace {

// Helper functions for little-endian serialization
void WriteUInt32LE(std::ostream& out, std::uint32_t value) {
  std::uint8_t bytes[4];
  bytes[0] = static_cast<std::uint8_t>(value & 0xFF);
  bytes[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
  bytes[2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
  bytes[3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
  out.write(reinterpret_cast<const char*>(bytes), 4);
}

void WriteUInt64LE(std::ostream& out, std::uint64_t value) {
  std::uint8_t bytes[8];
  for (int i = 0; i < 8; ++i) {
    bytes[i] = static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF);
  }
  out.write(reinterpret_cast<const char*>(bytes), 8);
}

void WriteDoubleLE(std::ostream& out, double value) {
  // Write double as raw bytes (little-endian platforms)
  static_assert(sizeof(double) == 8, "Double must be 8 bytes");
  out.write(reinterpret_cast<const char*>(&value), 8);
}

std::uint32_t ReadUInt32LE(std::istream& in) {
  std::uint8_t bytes[4];
  in.read(reinterpret_cast<char*>(bytes), 4);
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8) |
         (static_cast<std::uint32_t>(bytes[2]) << 16) |
         (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint64_t ReadUInt64LE(std::istream& in) {
  std::uint8_t bytes[8];
  in.read(reinterpret_cast<char*>(bytes), 8);
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(bytes[i]) << (i * 8);
  }
  return value;
}

double ReadDoubleLE(std::istream& in) {
  double value;
  in.read(reinterpret_cast<char*>(&value), 8);
  return value;
}

}  // namespace

// FImageHeader implementation

FImageHeader::FImageHeader()
    : width(0),
      height(0),
      channels(0),
      pixel_type(static_cast<std::uint8_t>(PixelType::kUInt8)),
      data_layout(static_cast<std::uint8_t>(DataLayout::kChannelsLast)),
      compression(static_cast<std::uint8_t>(CompressionType::kNone)),
      quantization_mode(0),
      tile_width(0),
      tile_height(0),
      num_tiles_x(0),
      num_tiles_y(0),
      mpp_x(0.0),
      mpp_y(0.0),
      data_offset(256),
      seek_table_offset(0) {
  // Initialize magic
  std::memcpy(magic.data(), "FIMAGE\x01\x00", 8);
  // Zero reserved area
  reserved.fill(0);
}

void FImageHeader::WriteTo(std::ostream& out) const {
  // Write magic (8 bytes)
  out.write(magic.data(), 8);

  // Write dimensions
  WriteUInt32LE(out, width);
  WriteUInt32LE(out, height);
  WriteUInt32LE(out, channels);

  // Write type and layout info
  out.write(reinterpret_cast<const char*>(&pixel_type), 1);
  out.write(reinterpret_cast<const char*>(&data_layout), 1);
  out.write(reinterpret_cast<const char*>(&compression), 1);
  out.write(reinterpret_cast<const char*>(&quantization_mode), 1);

  // Write tile info
  WriteUInt32LE(out, tile_width);
  WriteUInt32LE(out, tile_height);
  WriteUInt32LE(out, num_tiles_x);
  WriteUInt32LE(out, num_tiles_y);

  // Write MPP
  WriteDoubleLE(out, mpp_x);
  WriteDoubleLE(out, mpp_y);

  // Write offsets
  WriteUInt64LE(out, data_offset);
  WriteUInt64LE(out, seek_table_offset);

  // Write reserved area
  out.write(reinterpret_cast<const char*>(reserved.data()), 184);
}

bool FImageHeader::ReadFrom(std::istream& in) {
  // Read magic
  in.read(magic.data(), 8);
  if (!IsValid())
    return false;

  // Read dimensions
  width = ReadUInt32LE(in);
  height = ReadUInt32LE(in);
  channels = ReadUInt32LE(in);

  // Read type and layout info
  in.read(reinterpret_cast<char*>(&pixel_type), 1);
  in.read(reinterpret_cast<char*>(&data_layout), 1);
  in.read(reinterpret_cast<char*>(&compression), 1);
  in.read(reinterpret_cast<char*>(&quantization_mode), 1);

  // Read tile info
  tile_width = ReadUInt32LE(in);
  tile_height = ReadUInt32LE(in);
  num_tiles_x = ReadUInt32LE(in);
  num_tiles_y = ReadUInt32LE(in);

  // Read MPP
  mpp_x = ReadDoubleLE(in);
  mpp_y = ReadDoubleLE(in);

  // Read offsets
  data_offset = ReadUInt64LE(in);
  seek_table_offset = ReadUInt64LE(in);

  // Read reserved area
  in.read(reinterpret_cast<char*>(reserved.data()), 184);

  return in.good();
}

bool FImageHeader::IsValid() const {
  return std::memcmp(magic.data(), "FIMAGE\x01\x00", 8) == 0;
}

size_t FImageHeader::GetPixelSize() const {
  switch (static_cast<PixelType>(pixel_type)) {
    case PixelType::kUInt8:
      return 1;
    case PixelType::kUInt16:
      return 2;
    case PixelType::kFloat32:
      return 4;
    default:
      return 1;
  }
}

// SeekTableEntry implementation

void SeekTableEntry::WriteTo(std::ostream& out) const {
  WriteUInt64LE(out, offset);
  WriteUInt64LE(out, compressed_size);
}

bool SeekTableEntry::ReadFrom(std::istream& in) {
  offset = ReadUInt64LE(in);
  compressed_size = ReadUInt64LE(in);
  return in.good();
}

// FImageSink implementation

FImageSink FImageSink::Create(const fs::path& filename) {
  return FImageSink(filename, TileSize(0, 0), CompressionType::kNone, 0.0, 0.0);
}

FImageSink FImageSink::Create(const fs::path& filename,
                              const TileSize& tile_size,
                              CompressionType compression) {
  return FImageSink(filename, tile_size, compression, 0.0, 0.0);
}

FImageSink FImageSink::Create(const fs::path& filename,
                              const TileSize& tile_size,
                              CompressionType compression, double mpp_x,
                              double mpp_y) {
  return FImageSink(filename, tile_size, compression, mpp_x, mpp_y);
}

FImageSink::FImageSink(const fs::path& filename, const TileSize& tile_size,
                       CompressionType compression, double mpp_x, double mpp_y)
    : SinkBase<FImageSink>(filename),
      tile_size_(tile_size),
      compression_(compression),
      mpp_x_(mpp_x),
      mpp_y_(mpp_y) {}

FImageHeader FImageSink::CreateHeader(const ImageInfo& dims) const {
  FImageHeader header;

  // Set dimensions
  header.width = static_cast<std::uint32_t>(dims.GetWidth());
  header.height = static_cast<std::uint32_t>(dims.GetHeight());
  header.channels = static_cast<std::uint32_t>(dims.channels);

  // Set pixel type from input dimensions
  header.pixel_type = static_cast<std::uint8_t>(dims.pixel_type);

  // Set data layout
  header.data_layout = static_cast<std::uint8_t>(dims.layout);

  // Set compression
  header.compression = static_cast<std::uint8_t>(compression_);

  // Set quantization mode (reserved for future, always 0 for now)
  header.quantization_mode = 0;

  // Set tile info
  header.tile_width = static_cast<std::uint32_t>(tile_size_.width);
  header.tile_height = static_cast<std::uint32_t>(tile_size_.height);

  // num_tiles_x and num_tiles_y will be set by caller if in tiled mode
  header.num_tiles_x = 0;
  header.num_tiles_y = 0;

  // Set MPP
  header.mpp_x = mpp_x_;
  header.mpp_y = mpp_y_;

  // Set offsets
  header.data_offset = 256;      // Always immediately after header
  header.seek_table_offset = 0;  // Will be updated if compressed

  return header;
}

}  // namespace fim

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
 * @file compressor.cpp
 * @brief Implementation of compression utilities.
 * @author Jonas Teuwen
 * @date 2025
 */
#include "fim/utilities/compressor.h"

#include <lz4.h>
#include <zstd.h>

#include <string>

namespace fim {

aifocore::Result<std::vector<uint8_t>> NoCompressor::CompressImpl(
    const std::vector<uint8_t>& data) {
  return data;
}

aifocore::Result<std::vector<uint8_t>> LZ4Compressor::CompressImpl(
    const std::vector<uint8_t>& data) {
  // Calculate maximum compressed size
  int max_compressed_size = LZ4_compressBound(data.size());
  if (max_compressed_size <= 0) {
    return aifocore::Status(aifocore::StatusCode::kInternal,
                            "LZ4_compressBound returned invalid size");
  }

  std::vector<uint8_t> compressed(max_compressed_size);

  // Compress
  int compressed_size =
      LZ4_compress_default(reinterpret_cast<const char*>(data.data()),
                           reinterpret_cast<char*>(compressed.data()),
                           data.size(), max_compressed_size);

  if (compressed_size <= 0) {
    return aifocore::Status(aifocore::StatusCode::kInternal,
                            "LZ4 compression failed");
  }

  // Resize to actual size
  compressed.resize(compressed_size);
  return compressed;
}

aifocore::Result<std::vector<uint8_t>> ZstdCompressor::CompressImpl(
    const std::vector<uint8_t>& data) {
  // Calculate maximum compressed size
  size_t max_compressed_size = ZSTD_compressBound(data.size());
  std::vector<uint8_t> compressed(max_compressed_size);

  // Compress with default compression level (3)
  size_t compressed_size = ZSTD_compress(compressed.data(), max_compressed_size,
                                         data.data(), data.size(), 3);

  if (ZSTD_isError(compressed_size)) {
    return aifocore::Status(
        aifocore::StatusCode::kInternal,
        std::string("Zstd compression failed: ") +
            std::string(ZSTD_getErrorName(compressed_size)));
  }

  // Resize to actual size
  compressed.resize(compressed_size);
  return compressed;
}

aifocore::Result<std::vector<uint8_t>> CompressData(
    const std::vector<uint8_t>& data, CompressionType type) {
  switch (type) {
    case CompressionType::kNone: {
      NoCompressor compressor;
      return compressor.Compress(data);
    }
    case CompressionType::kLZ4: {
      LZ4Compressor compressor;
      return compressor.Compress(data);
    }
    case CompressionType::kZstd: {
      ZstdCompressor compressor;
      return compressor.Compress(data);
    }
    default:
      return aifocore::Status(aifocore::StatusCode::kInvalidArgument,
                              "Unsupported compression type");
  }
}

}  // namespace fim

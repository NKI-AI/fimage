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
 * @file decompressor.cpp
 * @brief Implementation of decompression utilities.
 * @author Jonas Teuwen
 * @date 2025
 */
#include "fim/utilities/decompressor.h"

#include <lz4.h>
#include <zstd.h>

#include <string>

namespace fim {

aifocore::Result<std::vector<uint8_t>> NoDecompressor::DecompressImpl(
    const std::vector<uint8_t>& compressed, size_t expected_size) {
  return compressed;
}

aifocore::Result<std::vector<uint8_t>> LZ4Decompressor::DecompressImpl(
    const std::vector<uint8_t>& compressed, size_t expected_size) {
  std::vector<uint8_t> decompressed(expected_size);

  int decompressed_size =
      LZ4_decompress_safe(reinterpret_cast<const char*>(compressed.data()),
                          reinterpret_cast<char*>(decompressed.data()),
                          compressed.size(), expected_size);

  if (decompressed_size < 0) {
    return aifocore::Status(aifocore::StatusCode::kInternal,
                            "LZ4 decompression failed");
  }

  if (static_cast<size_t>(decompressed_size) != expected_size) {
    return aifocore::Status(aifocore::StatusCode::kInternal,
                            "LZ4 decompressed size mismatch: expected " +
                                std::to_string(expected_size) + ", got " +
                                std::to_string(decompressed_size));
  }

  return decompressed;
}

aifocore::Result<std::vector<uint8_t>> ZstdDecompressor::DecompressImpl(
    const std::vector<uint8_t>& compressed, size_t expected_size) {
  std::vector<uint8_t> decompressed(expected_size);

  size_t decompressed_size = ZSTD_decompress(
      decompressed.data(), expected_size, compressed.data(), compressed.size());

  if (ZSTD_isError(decompressed_size)) {
    return aifocore::Status(
        aifocore::StatusCode::kInternal,
        std::string("Zstd decompression failed: ") +
            std::string(ZSTD_getErrorName(decompressed_size)));
  }

  if (decompressed_size != expected_size) {
    return aifocore::Status(aifocore::StatusCode::kInternal,
                            "Zstd decompressed size mismatch: expected " +
                                std::to_string(expected_size) + ", got " +
                                std::to_string(decompressed_size));
  }

  return decompressed;
}

aifocore::Result<std::vector<uint8_t>> DecompressData(
    const std::vector<uint8_t>& compressed, size_t expected_size,
    CompressionType type) {
  switch (type) {
    case CompressionType::kNone: {
      NoDecompressor decompressor;
      return decompressor.Decompress(compressed, expected_size);
    }
    case CompressionType::kLZ4: {
      LZ4Decompressor decompressor;
      return decompressor.Decompress(compressed, expected_size);
    }
    case CompressionType::kZstd: {
      ZstdDecompressor decompressor;
      return decompressor.Decompress(compressed, expected_size);
    }
    default:
      return aifocore::Status(aifocore::StatusCode::kInvalidArgument,
                              "Unsupported compression type");
  }
}

}  // namespace fim

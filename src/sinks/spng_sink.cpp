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
 * @file spng_sink.cpp
 * @brief Implementation of streaming PNG image sink using libspng for the fim
 * library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the implementation of the SpngSink class which provides
 * streaming PNG image writing capabilities using the libspng library. It
 * handles encoding images row-by-row for minimal memory usage.
 */
#include "fim/sinks/spng_sink.h"

namespace fim {

SpngSink SpngSink::Create(const fs::path& filename) {
  return SpngSink(filename);
}

SpngSink::SpngSink(const fs::path& filename) : SinkBase<SpngSink>(filename) {}

SpngSink::~SpngSink() = default;

SpngSink::SpngSink(SpngSink&& other) noexcept = default;

SpngSink& SpngSink::operator=(SpngSink&& other) noexcept = default;

spng_color_type SpngSink::MapChannelsToColorType(int channels) {
  switch (channels) {
    case 1:
      return SPNG_COLOR_TYPE_GRAYSCALE;
    case 3:
      return SPNG_COLOR_TYPE_TRUECOLOR;
    case 4:
      return SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
    default:
      throw std::runtime_error("Unsupported channel count for PNG output: " +
                               std::to_string(channels));
  }
}

}  // namespace fim

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
 * @file lodepng_png_sink.cpp
 * @brief Implementation of PNG image sink using lodepng for the fim library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains the implementation of the LodePngSink class which provides
 * PNG image writing capabilities using the lodepng library. It handles
 * assembling complete images from tiles and writing them to PNG files.
 */
#include "fim/sinks/lodepng_png_sink.h"

namespace fim {

LodePngSink LodePngSink::Create(const fs::path& filename) {
  return LodePngSink(filename);
}

LodePngSink::LodePngSink(const fs::path& filename)
    : SinkBase<LodePngSink>(filename) {}

}  // namespace fim

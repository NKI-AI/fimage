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
 * @file deferred_black_canvas.cpp
 * @brief Implementation of DeferredBlackCanvas class.
 * @author Jonas Teuwen
 * @date 2025
 */

#include "fim/python/deferred_black_canvas.h"

#include <stdexcept>

#include "fim/python/factory.h"
#include "fim/sources/black_source.h"

namespace fim {
namespace python {

DeferredBlackCanvas::DeferredBlackCanvas(int width, int height)
    : width_(width), height_(height) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument(
        "Canvas dimensions must be positive (width: " + std::to_string(width) +
        ", height: " + std::to_string(height) + ")");
  }
}

PyStage DeferredBlackCanvas::Paste(const PyStage& image, int x, int y) const {
  // Infer channel count from the pasted image
  auto img_dims = image.GetDimensions();
  int channels = img_dims.channels;

  // Create black source with inferred channels
  ImageInfo canvas_dims(width_, height_, channels, PixelType::kUInt8,
                        DataLayout::kChannelsLast);
  auto black_source = BlackSource::Create(canvas_dims);
  auto black_model =
      std::make_unique<StageModel<BlackSource>>(std::move(black_source));
  PyStage background(std::move(black_model));

  // Apply paste operation
  return ApplyPaste(background, image, x, y);
}

}  // namespace python
}  // namespace fim

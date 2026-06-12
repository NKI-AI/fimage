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
 * @file deferred_black_canvas.h
 * @brief Deferred black canvas for Python bindings.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file defines the DeferredBlackCanvas class which is a Python-only
 * helper that defers channel determination until the first paste operation.
 * This enables the API: fim.Image.black(width, height).paste(image, x, y)
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_PYTHON_DEFERRED_BLACK_CANVAS_H_
#define AIFO_FIMAGE_INCLUDE_FIM_PYTHON_DEFERRED_BLACK_CANVAS_H_

#include "fim/python/py_stage.h"

namespace fim {
namespace python {

/**
 * @brief Deferred black canvas for Python bindings.
 *
 * This class represents a black canvas whose channel count is not yet
 * determined. When the first paste() operation is called, the channel
 * count is inferred from the pasted image, a BlackSource is created,
 * and a Paste operator is applied.
 *
 * This enables a clean Python API:
 *   canvas = fim.Image.black(2048, 2048)
 *   result = canvas.paste(img1, 0, 0).paste(img2, 100, 100)
 */
class DeferredBlackCanvas {
 public:
  /**
   * @brief Constructs a deferred black canvas.
   *
   * @param width Canvas width in pixels
   * @param height Canvas height in pixels
   */
  DeferredBlackCanvas(int width, int height);

  /**
   * @brief Pastes an image onto the black canvas.
   *
   * This is the first paste operation, which infers the channel count from
   * the pasted image, creates a BlackSource, wraps it in a PyStage, and
   * applies the Paste operator.
   *
   * @param image The image to paste
   * @param x X coordinate where image top-left will be placed
   * @param y Y coordinate where image top-left will be placed
   * @return PyStage with the paste operation applied (can be chained)
   * @throw std::invalid_argument if the image extends beyond canvas bounds
   */
  PyStage Paste(const PyStage& image, int x, int y) const;

  /**
   * @brief Gets the canvas width.
   *
   * @return Canvas width in pixels
   */
  int GetWidth() const { return width_; }

  /**
   * @brief Gets the canvas height.
   *
   * @return Canvas height in pixels
   */
  int GetHeight() const { return height_; }

 private:
  int width_;   ///< Canvas width
  int height_;  ///< Canvas height
};

}  // namespace python
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_PYTHON_DEFERRED_BLACK_CANVAS_H_

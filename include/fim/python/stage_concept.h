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
 * @file stage_concept.h
 * @brief Type-erasure concept interface for fim pipeline stages.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file defines the StageConcept interface that provides type erasure
 * for CRTP-based pipeline stages. This allows Python bindings to work with
 * a stable ABI while preserving the lazy evaluation semantics of the C++
 * pipeline.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_PYTHON_STAGE_CONCEPT_H_
#define AIFO_FIMAGE_INCLUDE_FIM_PYTHON_STAGE_CONCEPT_H_

#include <memory>

#include "fim/types.h"

namespace fim {
namespace python {

/**
 * @brief Abstract interface for type-erased pipeline stages.
 *
 * This class provides a pure virtual interface that mirrors the CRTP
 * interface used by fim sources and operators. It allows different
 * CRTP instantiations to be stored polymorphically while maintaining
 * the lazy evaluation semantics.
 *
 * The interface operates purely on value types (Tile, ImageInfo,
 * TileSize) which are easy to pass across the Python/C++ boundary.
 */
class StageConcept {
 public:
  virtual ~StageConcept() = default;

  /**
   * @brief Gets the dimensions of the stage's output.
   *
   * @return ImageInfo containing width, height, and channels
   */
  virtual ImageInfo GetDimensions() const = 0;

  /**
   * @brief Gets the ideal tile size for processing this stage.
   *
   * @return TileSize containing the recommended tile dimensions
   */
  virtual TileSize GetIdealTileSize() const = 0;

  /**
   * @brief Gets a tile from the stage's output.
   *
   * This method retrieves a rectangular region of the stage's output.
   * The actual processing is deferred until this method is called.
   *
   * @param x X coordinate of the top-left corner
   * @param y Y coordinate of the top-left corner
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile containing the processed image data
   */
  virtual Tile GetTile(int x, int y, int width, int height) const = 0;
};

}  // namespace python
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_PYTHON_STAGE_CONCEPT_H_

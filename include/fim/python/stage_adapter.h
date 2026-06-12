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
 * @file stage_adapter.h
 * @brief Adapter to bridge CRTP operators and type-erased stages.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file defines PyStageAdapter which allows C++ CRTP operators to work
 * with type-erased StageConcept pointers. This enables reusing the tested
 * C++ operator implementations in Python bindings without duplicating logic.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_PYTHON_STAGE_ADAPTER_H_
#define AIFO_FIMAGE_INCLUDE_FIM_PYTHON_STAGE_ADAPTER_H_

#include <memory>
#include <utility>

#include "fim/python/stage_concept.h"
#include "fim/types.h"

namespace fim {
namespace python {

/**
 * @brief Adapter that allows CRTP operators to work with type-erased stages.
 *
 * This class implements the CRTP interface expected by fim operators
 * (GetDimensions, GetIdealTileSize, GetTile) by forwarding calls to a
 * type-erased StageConcept. This allows operators like Crop<PyStageAdapter>
 * to work with any pipeline stage without knowing its concrete type.
 *
 * The adapter can be stored by value (required by CRTP operators) while
 * sharing the underlying stage through a shared pointer.
 */
class PyStageAdapter {
 public:
  /**
   * @brief Default constructor creates an invalid adapter.
   *
   * This is needed for compatibility with CRTP operators that may
   * default-construct the input type. The adapter will be invalid
   * and should not be used until properly initialized.
   */
  PyStageAdapter() : stage_(nullptr) {}

  /**
   * @brief Constructs an adapter from a shared stage pointer.
   *
   * @param stage Shared pointer to the stage to adapt (must not be null)
   * @throw std::invalid_argument if stage is null
   */
  explicit PyStageAdapter(std::shared_ptr<const StageConcept> stage)
      : stage_(std::move(stage)) {
    if (!stage_) {
      throw std::invalid_argument("PyStageAdapter: stage cannot be null");
    }
  }

  /**
   * @brief Copy constructor.
   *
   * @param other The adapter to copy
   */
  PyStageAdapter(const PyStageAdapter& other) = default;

  /**
   * @brief Move constructor.
   *
   * @param other The adapter to move
   */
  PyStageAdapter(PyStageAdapter&& other) noexcept = default;

  /**
   * @brief Copy assignment operator.
   *
   * @param other The adapter to copy
   * @return Reference to this adapter
   */
  PyStageAdapter& operator=(const PyStageAdapter& other) = default;

  /**
   * @brief Move assignment operator.
   *
   * @param other The adapter to move
   * @return Reference to this adapter
   */
  PyStageAdapter& operator=(PyStageAdapter&& other) noexcept = default;

  /**
   * @brief Gets the dimensions by forwarding to the underlying stage.
   *
   * @return ImageInfo from the wrapped stage
   */
  ImageInfo GetDimensions() const { return stage_->GetDimensions(); }

  /**
   * @brief Gets the ideal tile size by forwarding to the underlying stage.
   *
   * @return TileSize from the wrapped stage
   */
  TileSize GetIdealTileSize() const { return stage_->GetIdealTileSize(); }

  /**
   * @brief Gets a tile by forwarding to the underlying stage.
   *
   * @param x X coordinate of the top-left corner
   * @param y Y coordinate of the top-left corner
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile from the wrapped stage
   */
  Tile GetTile(int x, int y, int width, int height) const {
    return stage_->GetTile(x, y, width, height);
  }

  /**
   * @brief Gets the underlying stage pointer.
   *
   * @return Shared pointer to the wrapped stage
   */
  std::shared_ptr<const StageConcept> GetStage() const { return stage_; }

 private:
  std::shared_ptr<const StageConcept>
      stage_;  ///< Shared pointer to the type-erased stage
};

}  // namespace python
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_PYTHON_STAGE_ADAPTER_H_

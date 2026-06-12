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
 * @file stage_model.h
 * @brief Type-erasure model implementation for fim pipeline stages.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file defines the StageModel template that wraps CRTP-based pipeline
 * stages and implements the StageConcept interface. This is the bridge between
 * the compile-time polymorphism of CRTP and the runtime polymorphism needed
 * for Python bindings.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_PYTHON_STAGE_MODEL_H_
#define AIFO_FIMAGE_INCLUDE_FIM_PYTHON_STAGE_MODEL_H_

#include <memory>
#include <utility>

#include "fim/python/stage_concept.h"
#include "fim/types.h"

namespace fim {
namespace python {

/**
 * @brief Type-erasure model that wraps CRTP pipeline stages.
 *
 * This template class stores a CRTP-based stage object by value and
 * forwards all StageConcept method calls to the underlying stage's
 * CRTP interface. This allows different pipeline configurations to
 * be stored polymorphically through the StageConcept interface.
 *
 * @tparam Stage The CRTP stage type (source or operator)
 */
template <typename Stage>
class StageModel : public StageConcept {
 public:
  /**
   * @brief Constructs a StageModel by moving a stage object.
   *
   * @param stage The CRTP stage object to wrap (moved)
   */
  explicit StageModel(Stage&& stage) : stage_(std::move(stage)) {}

  /**
   * @brief Copy constructor (deleted to prevent expensive copies).
   *
   * StageModel wraps move-only sources and operators. Copying would
   * either be ill-formed or produce expensive deep copies of pipeline stages.
   */
  StageModel(const StageModel& other) = delete;

  /**
   * @brief Copy assignment operator (deleted).
   */
  StageModel& operator=(const StageModel& other) = delete;

  /**
   * @brief Move constructor.
   *
   * @param other The StageModel to move
   */
  StageModel(StageModel&& other) noexcept = default;

  /**
   * @brief Move assignment operator.
   *
   * @param other The StageModel to move
   */
  StageModel& operator=(StageModel&& other) noexcept = default;

  /**
   * @brief Gets the dimensions by forwarding to the wrapped stage.
   *
   * @return ImageInfo from the underlying stage
   */
  ImageInfo GetDimensions() const override { return stage_.GetDimensions(); }

  /**
   * @brief Gets the ideal tile size by forwarding to the wrapped stage.
   *
   * @return TileSize from the underlying stage
   */
  TileSize GetIdealTileSize() const override {
    return stage_.GetIdealTileSize();
  }

  /**
   * @brief Gets a tile by forwarding to the wrapped stage.
   *
   * @param x X coordinate of the top-left corner
   * @param y Y coordinate of the top-left corner
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile from the underlying stage
   */
  Tile GetTile(int x, int y, int width, int height) const override {
    return stage_.GetTile(x, y, width, height);
  }

  /**
   * @brief Gets a const reference to the underlying stage.
   *
   * This is useful for advanced operations that need direct access
   * to the CRTP stage object.
   *
   * @return Const reference to the wrapped stage
   */
  const Stage& GetStage() const { return stage_; }

 private:
  Stage stage_;  ///< The wrapped CRTP stage object
};

}  // namespace python
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_PYTHON_STAGE_MODEL_H_

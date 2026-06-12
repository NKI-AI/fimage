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
 * @file py_stage.h
 * @brief Python-facing stage handle for fim pipeline.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file defines the PyStage class which is the main handle exposed to
 * Python. It wraps a shared pointer to a StageConcept, allowing multiple
 * Python references to share the same pipeline stage while preserving lazy
 * evaluation semantics.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_PYTHON_PY_STAGE_H_
#define AIFO_FIMAGE_INCLUDE_FIM_PYTHON_PY_STAGE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "fim/python/stage_concept.h"
#include "fim/types.h"

namespace fim {
namespace python {

/**
 * @brief Variant type for image properties exposed to Python.
 *
 * Properties are arbitrary key-value pairs that can hold basic scalar values.
 * This is intentionally schema-free: different readers can expose different
 * keys and semantics.
 */
using ImagePropertyValue = std::variant<int64_t, double, std::string>;
using ImageProperties = std::map<std::string, ImagePropertyValue>;

/**
 * @brief Python-facing handle for pipeline stages.
 *
 * This class wraps a shared pointer to a StageConcept, providing a stable
 * handle that can be copied and passed around in Python. Multiple PyStage
 * instances can reference the same underlying pipeline stage, and the lazy
 * evaluation DAG remains intact.
 *
 * This is the primary type exposed to Python as the `Image` class.
 */
class PyStage {
 public:
  /**
   * @brief Default constructor creates an empty stage.
   */
  PyStage() : stage_(nullptr) {}

  /**
   * @brief Constructs a PyStage from a StageConcept pointer.
   *
   * @param stage Unique pointer to a StageConcept implementation
   */
  explicit PyStage(std::unique_ptr<StageConcept>&& stage)
      : stage_(std::move(stage)) {}

  /**
   * @brief Constructs a PyStage from a StageConcept pointer and metadata.
   *
   * @param stage Unique pointer to a StageConcept implementation
   * @param properties Optional image properties to carry alongside the stage
   */
  PyStage(std::unique_ptr<StageConcept>&& stage,
          std::shared_ptr<const ImageProperties> properties)
      : stage_(std::move(stage)), properties_(std::move(properties)) {}

  /**
   * @brief Copy constructor shares the underlying stage.
   *
   * @param other The PyStage to copy
   */
  PyStage(const PyStage& other) = default;

  /**
   * @brief Move constructor.
   *
   * @param other The PyStage to move
   */
  PyStage(PyStage&& other) noexcept = default;

  /**
   * @brief Copy assignment operator.
   *
   * @param other The PyStage to copy
   * @return Reference to this PyStage
   */
  PyStage& operator=(const PyStage& other) = default;

  /**
   * @brief Move assignment operator.
   *
   * @param other The PyStage to move
   * @return Reference to this PyStage
   */
  PyStage& operator=(PyStage&& other) noexcept = default;

  /**
   * @brief Gets the dimensions of the stage's output.
   *
   * @return ImageInfo containing width, height, and channels
   * @throw std::runtime_error if the stage is empty
   */
  ImageInfo GetDimensions() const {
    CheckValid();
    return stage_->GetDimensions();
  }

  /**
   * @brief Gets the ideal tile size for processing this stage.
   *
   * @return TileSize containing the recommended tile dimensions
   * @throw std::runtime_error if the stage is empty
   */
  TileSize GetIdealTileSize() const {
    CheckValid();
    return stage_->GetIdealTileSize();
  }

  /**
   * @brief Gets a tile from the stage's output.
   *
   * @param x X coordinate of the top-left corner
   * @param y Y coordinate of the top-left corner
   * @param width Width of the requested tile
   * @param height Height of the requested tile
   * @return Tile containing the processed image data
   * @throw std::runtime_error if the stage is empty
   */
  Tile GetTile(int x, int y, int width, int height) const {
    CheckValid();
    return stage_->GetTile(x, y, width, height);
  }

  /**
   * @brief Checks if the stage is valid (non-null).
   *
   * @return true if the stage contains a valid pipeline stage
   */
  bool IsValid() const { return stage_ != nullptr; }

  /**
   * @brief Gets a const reference to the underlying StageConcept.
   *
   * This is useful for internal operations that need direct access
   * to the StageConcept interface.
   *
   * @return Const reference to the underlying stage
   * @throw std::runtime_error if the stage is empty
   */
  const StageConcept& GetStageConcept() const {
    CheckValid();
    return *stage_;
  }

  /**
   * @brief Gets the shared pointer to the underlying StageConcept.
   *
   * This is useful for creating operators that need to share ownership
   * of the input stage. Validates the stage before returning to prevent
   * null pointer dereferences.
   *
   * @return Shared pointer to the underlying stage
   * @throw std::runtime_error if the stage is empty
   */
  std::shared_ptr<StageConcept> GetStagePtr() const {
    CheckValid();
    return stage_;
  }

  /**
   * @brief Returns the optional image properties associated with this stage.
   */
  std::shared_ptr<const ImageProperties> GetPropertiesPtr() const {
    return properties_;
  }

 private:
  /**
   * @brief Checks if the stage is valid and throws if not.
   *
   * @throw std::runtime_error if the stage is empty
   */
  void CheckValid() const {
    if (!stage_) {
      throw std::runtime_error("Invalid PyStage: stage is null");
    }
  }

  std::shared_ptr<StageConcept>
      stage_;  ///< Shared pointer to the pipeline stage
  std::shared_ptr<const ImageProperties> properties_;
};

}  // namespace python
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_PYTHON_PY_STAGE_H_

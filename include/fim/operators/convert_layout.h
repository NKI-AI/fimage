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
 * @file convert_layout.h
 * @brief Operator to convert between DataLayout modes (channels-first/last).
 * @author Jonas Teuwen
 * @date 2025
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_CONVERT_LAYOUT_H_
#define AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_CONVERT_LAYOUT_H_

#include <utility>

#include "fim/pipeline.h"
#include "fim/types.h"
#include "fim/utilities/layout_utils.h"

namespace fim {

/**
 * @brief ConvertLayout operator: converts tile memory layout lazily on demand.
 *
 * This operator preserves width/height/channels/pixel_type. It changes only
 * the `DataLayout` of the output tiles by reordering values.
 */
template <typename InputType>
class ConvertLayout : public OperatorBase<ConvertLayout<InputType>, InputType> {
 public:
  ConvertLayout(InputType input, DataLayout target_layout)
      : OperatorBase<ConvertLayout<InputType>, InputType>(std::move(input)),
        target_layout_(target_layout) {}

  ImageInfo GetDimensions() const {
    auto in_dims = this->input_.GetDimensions();
    return ImageInfo(in_dims.GetWidth(), in_dims.GetHeight(), in_dims.channels,
                     in_dims.pixel_type, target_layout_);
  }

  TileSize GetIdealTileSize() const { return this->input_.GetIdealTileSize(); }

  Tile GetTile(int x, int y, int width, int height) const {
    Tile tile = this->input_.GetTile(x, y, width, height);
    return layout_utils::ConvertTileDataLayout(tile, target_layout_);
  }

 private:
  DataLayout target_layout_;
};

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_CONVERT_LAYOUT_H_

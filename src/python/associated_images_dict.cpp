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

#include "fim/python/associated_images_dict.h"

#include <algorithm>
#include <stdexcept>

#include "fim/python/factory.h"
#include "fim/python/stage_model.h"
#include "fim/sources/associated_image_source.h"

namespace fim {
namespace python {

// ============================================================================
// FastSlideAssociatedImagesDict Implementation
// ============================================================================

void FastSlideAssociatedImagesDict::EnsureNamesLoaded() const {
  if (!names_loaded_) {
    if (!source_) {
      throw std::runtime_error("Context has been closed");
    }
    available_names_ = source_->GetAssociatedImageNames();
    names_loaded_ = true;
  }
}

PyStage FastSlideAssociatedImagesDict::GetItem(const std::string& name) const {
  EnsureNamesLoaded();

  // Check if image exists
  if (std::find(available_names_.begin(), available_names_.end(), name) ==
      available_names_.end()) {
    throw std::runtime_error("Associated image '" + name + "' not found");
  }

  if (!source_) {
    throw std::runtime_error("Context has been closed");
  }

  // Create and return lazy fim.Image
  auto assoc_source = FastSlideAssociatedImageSource::Create(source_, name);
  auto model = std::make_unique<StageModel<FastSlideAssociatedImageSource>>(
      std::move(assoc_source));
  return PyStage(std::move(model));
}

bool FastSlideAssociatedImagesDict::Contains(const std::string& name) const {
  EnsureNamesLoaded();
  return std::find(available_names_.begin(), available_names_.end(), name) !=
         available_names_.end();
}

std::vector<std::string> FastSlideAssociatedImagesDict::Keys() const {
  EnsureNamesLoaded();
  return available_names_;
}

size_t FastSlideAssociatedImagesDict::Len() const {
  EnsureNamesLoaded();
  return available_names_.size();
}

// ============================================================================
// OpenSlideAssociatedImagesDict Implementation
// ============================================================================

#if defined(FIM_WITH_OPENSLIDE)
void OpenSlideAssociatedImagesDict::EnsureNamesLoaded() const {
  if (!names_loaded_) {
    if (!source_) {
      throw std::runtime_error("Context has been closed");
    }
    available_names_ = source_->GetAssociatedImageNames();
    names_loaded_ = true;
  }
}

PyStage OpenSlideAssociatedImagesDict::GetItem(const std::string& name) const {
  EnsureNamesLoaded();

  // Check if image exists
  if (std::find(available_names_.begin(), available_names_.end(), name) ==
      available_names_.end()) {
    throw std::runtime_error("Associated image '" + name + "' not found");
  }

  if (!source_) {
    throw std::runtime_error("Context has been closed");
  }

  // Create and return lazy fim.Image
  auto assoc_source = OpenSlideAssociatedImageSource::Create(source_, name);
  auto model = std::make_unique<StageModel<OpenSlideAssociatedImageSource>>(
      std::move(assoc_source));
  return PyStage(std::move(model));
}

bool OpenSlideAssociatedImagesDict::Contains(const std::string& name) const {
  EnsureNamesLoaded();
  return std::find(available_names_.begin(), available_names_.end(), name) !=
         available_names_.end();
}

std::vector<std::string> OpenSlideAssociatedImagesDict::Keys() const {
  EnsureNamesLoaded();
  return available_names_;
}

size_t OpenSlideAssociatedImagesDict::Len() const {
  EnsureNamesLoaded();
  return available_names_.size();
}
#endif  // defined(FIM_WITH_OPENSLIDE)

}  // namespace python
}  // namespace fim

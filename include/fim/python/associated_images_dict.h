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
 * @file associated_images_dict.h
 * @brief Lazy-loading dictionary wrapper for associated images.
 * @author Jonas Teuwen
 * @date 2025
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_PYTHON_ASSOCIATED_IMAGES_DICT_H_
#define AIFO_FIMAGE_INCLUDE_FIM_PYTHON_ASSOCIATED_IMAGES_DICT_H_

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace fim {

// Forward declarations from sources
class FastSlideSource;
class OpenSlideSource;

namespace python {

// Forward declaration
class PyStage;

/**
 * @brief Lazy dict wrapper for FastSlide associated images.
 *
 * Only creates fim.Image objects when accessed via __getitem__,
 * not when the dict is created. Holds a shared_ptr to the source
 * so it can outlive the context manager.
 */
class FastSlideAssociatedImagesDict {
 public:
  explicit FastSlideAssociatedImagesDict(
      std::shared_ptr<FastSlideSource> source)
      : source_(std::move(source)), names_loaded_(false) {}

  PyStage GetItem(const std::string& name) const;
  bool Contains(const std::string& name) const;
  std::vector<std::string> Keys() const;
  size_t Len() const;

 private:
  void EnsureNamesLoaded() const;

  std::shared_ptr<FastSlideSource> source_;
  mutable std::vector<std::string> available_names_;
  mutable bool names_loaded_;
};

#if defined(FIM_WITH_OPENSLIDE)
/**
 * @brief Lazy dict wrapper for OpenSlide associated images.
 *
 * Only creates fim.Image objects when accessed via __getitem__,
 * not when the dict is created. Holds a shared_ptr to the source
 * so it can outlive the context manager.
 */
class OpenSlideAssociatedImagesDict {
 public:
  explicit OpenSlideAssociatedImagesDict(
      std::shared_ptr<OpenSlideSource> source)
      : source_(std::move(source)), names_loaded_(false) {}

  PyStage GetItem(const std::string& name) const;
  bool Contains(const std::string& name) const;
  std::vector<std::string> Keys() const;
  size_t Len() const;

 private:
  void EnsureNamesLoaded() const;

  std::shared_ptr<OpenSlideSource> source_;
  mutable std::vector<std::string> available_names_;
  mutable bool names_loaded_;
};
#endif  // defined(FIM_WITH_OPENSLIDE)

}  // namespace python
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_PYTHON_ASSOCIATED_IMAGES_DICT_H_

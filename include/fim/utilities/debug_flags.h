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
 * @file debug_flags.h
 * @brief Small helpers for opt-in debug logging/behavior via environment vars.
 * @author Jonas Teuwen
 * @date 2025
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_DEBUG_FLAGS_H_
#define AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_DEBUG_FLAGS_H_

#include <cstdlib>
#include <string_view>

namespace fim::debug_flags {

/**
 * @brief Returns true if the given environment variable is set to a truthy
 * value.
 *
 * Truthy values: "1", "true", "TRUE", "yes", "YES".
 */
inline bool IsEnabled(std::string_view env_var_name) {
  const char* value = std::getenv(env_var_name.data());
  if (value == nullptr) {
    return false;
  }
  const std::string_view v(value);
  return v == "1" || v == "true" || v == "TRUE" || v == "yes" || v == "YES";
}

}  // namespace fim::debug_flags

#endif  // AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_DEBUG_FLAGS_H_

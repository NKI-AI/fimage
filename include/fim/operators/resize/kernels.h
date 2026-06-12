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
 * @file kernels.h
 * @brief Resampling kernels for image resize operations.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains kernel implementations for high-quality image resampling,
 * including Lanczos and Magic2021 kernels. These kernels are used in separable
 * convolution for resizing images with minimal artifacts.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_RESIZE_KERNELS_H_
#define AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_RESIZE_KERNELS_H_

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <type_traits>

namespace fim {
namespace resize {

/**
 * @brief Enumeration of available resampling kernel types.
 */
enum class KernelType : std::uint8_t {
  kNearest,   ///< Nearest neighbor (no interpolation)
  kLanczos2,  ///< Lanczos kernel with radius 2
  kLanczos3,  ///< Lanczos kernel with radius 3 (default)
  kMagic2021  ///< Magic Kernel Sharp 2021 with radius 4.5
};

/**
 * @brief Clamp a value to the valid range for type T.
 *
 * @tparam T Target type
 * @param value Value to clamp
 * @return Clamped value of type T
 */
template <typename T>
constexpr T ClampValue(double value) noexcept {
  if constexpr (std::is_floating_point_v<T>) {
    return static_cast<T>(value);
  } else if constexpr (std::is_unsigned_v<T>) {
    constexpr double kMaxVal =
        static_cast<double>(std::numeric_limits<T>::max());
    return static_cast<T>(std::clamp(value, 0.0, kMaxVal));
  } else {
    constexpr double kMinVal =
        static_cast<double>(std::numeric_limits<T>::min());
    constexpr double kMaxVal =
        static_cast<double>(std::numeric_limits<T>::max());
    return static_cast<T>(std::clamp(value, kMinVal, kMaxVal));
  }
}

/**
 * @brief Convert a double value to pixel type with proper rounding.
 *
 * For integral types, uses nearest-neighbor rounding to avoid truncation bias.
 * For floating-point types, performs a direct cast.
 *
 * @tparam T Target pixel type
 * @param value Value to convert
 * @return Converted and clamped value of type T
 */
template <typename T>
inline T ToPixel(double value) noexcept {
  if constexpr (std::is_integral_v<T>) {
    return ClampValue<T>(std::nearbyint(value));
  }
  return static_cast<T>(value);
}

/**
 * @brief Reflect an index at image boundaries (symmetric extension).
 *
 * This function implements mirror padding at boundaries using a periodic
 * mirror that works for any integer offset, not just one bounce. This is
 * robust against bugs or large kernel radii that might request indices
 * far outside the valid range.
 *
 * @param idx Index to reflect
 * @param max Maximum valid index (exclusive)
 * @return Reflected index in range [0, max)
 */
constexpr int ReflectIndex(int idx, int max) noexcept {
  if (max <= 1) {
    return 0;
  }
  const int period = 2 * (max - 1);
  int temp = idx % period;
  if (temp < 0) {
    temp += period;
  }
  return (temp < max) ? temp : period - temp;
}

/**
 * @brief Compute sin(π*x) with better numerical accuracy.
 *
 * This helper function reduces cancellation errors for small x.
 *
 * @param x Input value
 * @return sin(π*x)
 */
inline double SinPi(double x) noexcept {
  return std::sin(std::numbers::pi * x);
}

/**
 * @brief Compute sinc function: sin(π*x) / (π*x).
 *
 * @param x Input value
 * @return Sinc of x
 */
constexpr double Sinc(double x) noexcept {
  if (x == 0.0) {
    return 1.0;
  }
  const double pi_x = std::numbers::pi * x;
  return SinPi(x) / pi_x;
}

/**
 * @brief Lanczos kernel function.
 *
 * @tparam A Lanczos radius (2 or 3)
 * @param x Distance from sample point
 * @return Kernel weight at distance x
 */
template <int A>

  requires(A >= 1 && A <= 5)
constexpr double LanczosKernel(double x) noexcept {
  if (std::abs(x) >= A) {
    return 0.0;
  }
  return Sinc(x) * Sinc(x / A);
}

/**
 * @brief Magic Kernel Sharp 2021 function.
 *
 * This is a piecewise polynomial kernel designed for sharp, high-quality
 * resampling with minimal ringing artifacts. Originally developed for
 * subpixel shifts in whole slide imaging.
 *
 * @param x Distance from sample point
 * @return Kernel weight at distance x
 */
inline double MagicKernel2021(double x) noexcept {
  x = std::abs(x);
  if (x <= 0.5) {
    return (577.0 / 576.0) - ((239.0 / 144.0) * x * x);
  }
  if (x <= 1.5) {
    return (1.0 / 144.0) * ((140.0 * x * x) - (379.0 * x) + 239.0);
  }
  if (x <= 2.5) {
    return -(1.0 / 144.0) * ((24.0 * x * x) - (113.0 * x) + 130.0);
  }
  if (x <= 3.5) {
    return (1.0 / 144.0) * ((4.0 * x * x) - (27.0 * x) + 45.0);
  }
  if (x <= 4.5) {
    const double temp = (2.0 * x) - 9.0;
    return -(1.0 / 1152.0) * temp * temp;
  }
  return 0.0;
}

/**
 * @brief Kernel configuration traits for compile-time optimization.
 *
 * @tparam K Kernel type
 */
template <KernelType K>
struct KernelTraits;

template <>
struct KernelTraits<KernelType::kNearest> {
  static constexpr int kRadius = 0;
  static constexpr double kSupport = 0.5;

  static double ComputeWeight(double dist) noexcept {
    // Nearest neighbor: return 1.0 if within half pixel, 0.0 otherwise
    return (std::abs(dist) <= 0.5) ? 1.0 : 0.0;
  }
};

template <>
struct KernelTraits<KernelType::kLanczos2> {
  static constexpr int kRadius = 2;
  static constexpr double kSupport = 2.0;

  static double ComputeWeight(double dist) noexcept {
    return LanczosKernel<2>(dist);
  }
};

template <>
struct KernelTraits<KernelType::kLanczos3> {
  static constexpr int kRadius = 3;
  static constexpr double kSupport = 3.0;

  static double ComputeWeight(double dist) noexcept {
    return LanczosKernel<3>(dist);
  }
};

template <>
struct KernelTraits<KernelType::kMagic2021> {
  static constexpr int kRadius = 5;  // ceil(4.5)
  static constexpr double kSupport = 4.5;

  static double ComputeWeight(double dist) noexcept {
    return MagicKernel2021(dist);
  }
};

/**
 * @brief Get the kernel radius for a given kernel type.
 *
 * @param kernel Kernel type
 * @return Kernel radius (integer ceiling of support)
 */
inline int GetKernelRadius(KernelType kernel) noexcept {
  switch (kernel) {
    case KernelType::kNearest:
      return KernelTraits<KernelType::kNearest>::kRadius;
    case KernelType::kLanczos2:
      return KernelTraits<KernelType::kLanczos2>::kRadius;
    case KernelType::kLanczos3:
      return KernelTraits<KernelType::kLanczos3>::kRadius;
    case KernelType::kMagic2021:
      return KernelTraits<KernelType::kMagic2021>::kRadius;
    default:
      return 3;
  }
}

/**
 * @brief Get the kernel support for a given kernel type.
 *
 * @param kernel Kernel type
 * @return Kernel support (half-width of non-zero region)
 */
inline double GetKernelSupport(KernelType kernel) noexcept {
  switch (kernel) {
    case KernelType::kNearest:
      return KernelTraits<KernelType::kNearest>::kSupport;
    case KernelType::kLanczos2:
      return KernelTraits<KernelType::kLanczos2>::kSupport;
    case KernelType::kLanczos3:
      return KernelTraits<KernelType::kLanczos3>::kSupport;
    case KernelType::kMagic2021:
      return KernelTraits<KernelType::kMagic2021>::kSupport;
    default:
      return 3.0;
  }
}

/**
 * @brief Compute kernel weight at given distance.
 *
 * @param kernel Kernel type
 * @param dist Distance from sample point
 * @return Kernel weight at distance dist
 */
inline double ComputeKernelWeight(KernelType kernel, double dist) noexcept {
  switch (kernel) {
    case KernelType::kNearest:
      return KernelTraits<KernelType::kNearest>::ComputeWeight(dist);
    case KernelType::kLanczos2:
      return KernelTraits<KernelType::kLanczos2>::ComputeWeight(dist);
    case KernelType::kLanczos3:
      return KernelTraits<KernelType::kLanczos3>::ComputeWeight(dist);
    case KernelType::kMagic2021:
      return KernelTraits<KernelType::kMagic2021>::ComputeWeight(dist);
    default:
      return KernelTraits<KernelType::kLanczos3>::ComputeWeight(dist);
  }
}

}  // namespace resize
}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_OPERATORS_RESIZE_KERNELS_H_

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

#include <vips/vips8>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <string>

#include "benchmark/benchmark.h"

namespace {

std::string GetBenchmarkFilename() {
  const char* env_file = std::getenv("BENCHMARK_FILENAME");
  if (env_file != nullptr) {
    return std::string(env_file);
  }
  // Fallback to default file
  return "/Users/jonasteuwen/data/T82-06575 H2 HE.mrxs";
}

/// @brief Libvips wrapper for benchmarking center crop operations
class VipsCropReader {
 public:
  explicit VipsCropReader(const std::string& filename) : filename_(filename) {}

  bool Open() {
    // Initialize VIPS only once globally
    static bool vips_initialized = []() {
      if (VIPS_INIT("vips_benchmark") != 0) {
        std::cerr << "Failed to initialize VIPS library\n";
        return false;
      }
      vips_concurrency_set(1);  // Single-threaded for consistent benchmarking
      return true;
    }();

    if (!vips_initialized) {
      std::cerr << "VIPS not initialized\n";
      return false;
    }

    // Verify the file can be opened
    try {
      auto test_image = vips::VImage::openslideload(filename_.c_str());
      return true;
    } catch (const vips::VError& e) {
      std::cerr << "Failed to open file '" << filename_ << "': " << e.what()
                << "\n";
      return false;
    }
  }

  const std::string& GetFilename() const { return filename_; }

  int GetLevelCount() const {
    try {
      auto image = vips::VImage::openslideload(filename_.c_str());
      if (image.get_typeof("openslide.level-count") != 0) {
        return image.get_int("openslide.level-count");
      }
    } catch (...) {
      return 0;
    }
    return 0;
  }

  bool GetLevelDimensions(int level, int64_t* width, int64_t* height) const {
    try {
      // Load the specific level directly to get its dimensions
      auto level_image = vips::VImage::openslideload(
          filename_.c_str(), vips::VImage::option()->set("level", level));
      *width = level_image.width();
      *height = level_image.height();
      return true;
    } catch (const vips::VError& e) {
      std::cerr << "Failed to get dimensions for level " << level << ": "
                << e.what() << "\n";
      return false;
    }
  }

  bool ReadCropAndWrite(int level, const std::string& output_file) {
    try {
      // Load the specific level
      auto level_image = vips::VImage::openslideload(
          filename_.c_str(), vips::VImage::option()->set("level", level));

      // Calculate center crop (1/4 of dimensions, centered at 3/8 position)
      int64_t width = level_image.width();
      int64_t height = level_image.height();
      int64_t crop_width = width / 4;
      int64_t crop_height = height / 4;
      int64_t offset_x = width * 3 / 8;
      int64_t offset_y = height * 3 / 8;

      // Crop the center region
      auto cropped = level_image.crop(
          static_cast<int>(offset_x), static_cast<int>(offset_y),
          static_cast<int>(crop_width), static_cast<int>(crop_height));

      // Write to TIFF
      cropped.tiffsave(output_file.c_str());

      return true;
    } catch (...) {
      return false;
    }
  }

 private:
  std::string filename_;
};

/// @brief Benchmark fixture for libvips center crop operations
class VipsCropFixture : public benchmark::Fixture {
 public:
  void SetUp(const ::benchmark::State& state) override {
    if (!reader_) {
      std::string filename = GetBenchmarkFilename();
      reader_ = std::make_unique<VipsCropReader>(filename);
      init_success_ = reader_->Open();
      if (!init_success_) {
        std::cerr << "SetUp failed: init_success_ is false\n";
        return;
      }
    }

    level_ = static_cast<int>(state.range(0));

    // Get level dimensions
    if (!reader_->GetLevelDimensions(level_, &level_width_, &level_height_)) {
      std::cerr << "Failed to get level dimensions for level " << level_
                << "\n";
      init_success_ = false;
      return;
    }

    // Calculate crop dimensions for metrics
    crop_width_ = level_width_ / 4;
    crop_height_ = level_height_ / 4;

    // Generate output filename
    output_file_ = "vips_level_" + std::to_string(level_) + ".tiff";
  }

  void TearDown(const ::benchmark::State& state) override {
    // Cleanup is automatic with RAII
  }

 protected:
  std::unique_ptr<VipsCropReader> reader_;
  bool init_success_{false};
  int level_{0};
  int64_t level_width_{0};
  int64_t level_height_{0};
  int64_t crop_width_{0};
  int64_t crop_height_{0};
  std::string output_file_;
};

/// @brief Libvips center crop benchmark
BENCHMARK_DEFINE_F(VipsCropFixture, CenterCrop)

(benchmark::State& state) {
  if (!init_success_) {
    state.SkipWithError("Failed to initialize libvips reader");
    return;
  }

  // Calculate center crop (1/4 of dimensions, centered at 3/8 position)
  int64_t offset_x = level_width_ * 3 / 8;
  int64_t offset_y = level_height_ * 3 / 8;

  int64_t total_bytes = 0;

  for (auto _ : state) {
    try {
      // Load the specific level
      auto level_image = vips::VImage::openslideload(
          reader_->GetFilename().c_str(),
          vips::VImage::option()->set("level", level_));

      // Crop the center region
      auto cropped = level_image.crop(
          static_cast<int>(offset_x), static_cast<int>(offset_y),
          static_cast<int>(crop_width_), static_cast<int>(crop_height_));

      // Write to TIFF
      cropped.tiffsave(output_file_.c_str());
    } catch (...) {
      state.SkipWithError("Failed to load, crop, and write");
      break;
    }

    // Track bytes processed (RGBA pixels are 4 bytes each)
    total_bytes += crop_width_ * crop_height_ * sizeof(uint32_t);
  }

  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(total_bytes);
}

// Register benchmarks for different levels
// Format: ->Args({level})

// Level 0 (highest resolution)
BENCHMARK_REGISTER_F(VipsCropFixture, CenterCrop)
    ->Args({0})
    ->Unit(benchmark::kMicrosecond);

// Level 1
BENCHMARK_REGISTER_F(VipsCropFixture, CenterCrop)
    ->Args({1})
    ->Unit(benchmark::kMicrosecond);

// Level 2
BENCHMARK_REGISTER_F(VipsCropFixture, CenterCrop)
    ->Args({2})
    ->Unit(benchmark::kMicrosecond);

/// @brief Libvips random crop benchmark
BENCHMARK_DEFINE_F(VipsCropFixture, RandomCrop)

(benchmark::State& state) {
  if (!init_success_) {
    state.SkipWithError("Failed to initialize libvips reader");
    return;
  }

  // Fixed seed for reproducible benchmarks
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> x_dist(
      0, std::max(int64_t{0}, level_width_ - crop_width_));
  std::uniform_int_distribution<int64_t> y_dist(
      0, std::max(int64_t{0}, level_height_ - crop_height_));

  int64_t total_bytes = 0;

  for (auto _ : state) {
    // Generate random crop position
    int64_t offset_x = x_dist(rng);
    int64_t offset_y = y_dist(rng);

    try {
      // Load the specific level
      auto level_image = vips::VImage::openslideload(
          reader_->GetFilename().c_str(),
          vips::VImage::option()->set("level", level_));

      // Crop at random position
      auto cropped = level_image.crop(
          static_cast<int>(offset_x), static_cast<int>(offset_y),
          static_cast<int>(crop_width_), static_cast<int>(crop_height_));

      // Write to TIFF
      cropped.tiffsave(output_file_.c_str());
    } catch (...) {
      state.SkipWithError("Failed to load, crop, and write");
      break;
    }

    // Track bytes processed (RGBA pixels are 4 bytes each)
    total_bytes += crop_width_ * crop_height_ * sizeof(uint32_t);
  }

  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(total_bytes);
}

// Random crop benchmarks - Level 0
BENCHMARK_REGISTER_F(VipsCropFixture, RandomCrop)
    ->Args({0})
    ->Unit(benchmark::kMicrosecond);

// Level 1
BENCHMARK_REGISTER_F(VipsCropFixture, RandomCrop)
    ->Args({1})
    ->Unit(benchmark::kMicrosecond);

// Level 2
BENCHMARK_REGISTER_F(VipsCropFixture, RandomCrop)
    ->Args({2})
    ->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();

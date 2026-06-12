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

#include <fim/image.h>
#include <fim/operators/crop.h>
#include <fim/sinks/tiff_sink.h>
#include <fim/sources/fastslide_source.h>

#include <cstdint>
#include <cstdlib>
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

/// @brief FImage wrapper for benchmarking center crop operations
class FImageCropReader {
 public:
  explicit FImageCropReader(const std::string& filename)
      : filename_(filename) {}

  bool Open() {
    // Verify the file can be opened
    try {
      auto test_source = fim::FastSlideSource::Create(filename_);
      return true;
    } catch (...) {
      return false;
    }
  }

  const std::string& GetFilename() const { return filename_; }

  bool GetLevelDimensions(int level, uint32_t* width, uint32_t* height,
                          uint32_t* channels) const {
    try {
      auto source = fim::FastSlideSource::Create(filename_);
      auto dims = source.GetLevelDimensions(level);
      *width = dims.width;
      *height = dims.height;
      *channels = source.GetNumChannels();
      return true;
    } catch (...) {
      return false;
    }
  }

 private:
  std::string filename_;
};

/// @brief Benchmark fixture for fimage center crop operations
class FImageCropFixture : public benchmark::Fixture {
 public:
  void SetUp(const ::benchmark::State& state) override {
    if (!reader_) {
      reader_ = std::make_unique<FImageCropReader>(GetBenchmarkFilename());
      init_success_ = reader_->Open();
      if (!init_success_) {
        return;
      }
    }

    level_ = static_cast<int>(state.range(0));

    // Get level dimensions
    uint32_t channels = 0;
    if (!reader_->GetLevelDimensions(level_, &level_width_, &level_height_,
                                     &channels)) {
      init_success_ = false;
      return;
    }

    // Calculate crop dimensions for metrics
    crop_width_ = level_width_ / 4;
    crop_height_ = level_height_ / 4;

    // Generate output filename
    output_file_ = "fimage_level_" + std::to_string(level_) + ".tiff";
  }

  void TearDown(const ::benchmark::State& state) override {
    // Cleanup is automatic with RAII
  }

 protected:
  std::unique_ptr<FImageCropReader> reader_;
  bool init_success_{false};
  int level_{0};
  uint32_t level_width_{0};
  uint32_t level_height_{0};
  uint32_t crop_width_{0};
  uint32_t crop_height_{0};
  std::string output_file_;
};

/// @brief FImage center crop benchmark
BENCHMARK_DEFINE_F(FImageCropFixture, CenterCrop)

(benchmark::State& state) {
  if (!init_success_) {
    state.SkipWithError("Failed to initialize fimage reader");
    return;
  }

  // Calculate center crop (1/4 of dimensions, centered at 3/8 position)
  uint32_t offset_x = level_width_ * 3 / 8;
  uint32_t offset_y = level_height_ * 3 / 8;

  int64_t total_bytes = 0;

  for (auto _ : state) {
    try {
      // Load the specific level
      auto source = fim::FastSlideSource::Create(reader_->GetFilename());
      auto level_view = source.LevelView(level_);

      // Crop the center region
      fim::Image(std::move(level_view))
          .Crop(static_cast<int>(offset_x), static_cast<int>(offset_y),
                static_cast<int>(crop_width_), static_cast<int>(crop_height_))
          .Render(fim::TiffSink::Create(output_file_));
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
BENCHMARK_REGISTER_F(FImageCropFixture, CenterCrop)
    ->Args({0})
    ->Unit(benchmark::kMicrosecond);

// Level 1
BENCHMARK_REGISTER_F(FImageCropFixture, CenterCrop)
    ->Args({1})
    ->Unit(benchmark::kMicrosecond);

// Level 2
BENCHMARK_REGISTER_F(FImageCropFixture, CenterCrop)
    ->Args({2})
    ->Unit(benchmark::kMicrosecond);

/// @brief FImage random crop benchmark
BENCHMARK_DEFINE_F(FImageCropFixture, RandomCrop)

(benchmark::State& state) {
  if (!init_success_) {
    state.SkipWithError("Failed to initialize fimage reader");
    return;
  }

  // Fixed seed for reproducible benchmarks
  std::mt19937 rng(42);
  std::uniform_int_distribution<uint32_t> x_dist(
      0, std::max(uint32_t{0}, level_width_ - crop_width_));
  std::uniform_int_distribution<uint32_t> y_dist(
      0, std::max(uint32_t{0}, level_height_ - crop_height_));

  int64_t total_bytes = 0;

  for (auto _ : state) {
    // Generate random crop position
    uint32_t offset_x = x_dist(rng);
    uint32_t offset_y = y_dist(rng);

    try {
      // Load the specific level
      auto source = fim::FastSlideSource::Create(reader_->GetFilename());
      auto level_view = source.LevelView(level_);

      // Crop at random position
      fim::Image(std::move(level_view))
          .Crop(static_cast<int>(offset_x), static_cast<int>(offset_y),
                static_cast<int>(crop_width_), static_cast<int>(crop_height_))
          .Render(fim::TiffSink::Create(output_file_));
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
BENCHMARK_REGISTER_F(FImageCropFixture, RandomCrop)
    ->Args({0})
    ->Unit(benchmark::kMicrosecond);

// Level 1
BENCHMARK_REGISTER_F(FImageCropFixture, RandomCrop)
    ->Args({1})
    ->Unit(benchmark::kMicrosecond);

// Level 2
BENCHMARK_REGISTER_F(FImageCropFixture, RandomCrop)
    ->Args({2})
    ->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();

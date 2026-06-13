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

#include <cstdlib>
#include <memory>
#include <string>

#include "benchmark/benchmark.h"
#include "fim/image.h"
#include "fim/operators/resize.h"
#include "fim/sinks/memory_sink.h"
#include "fim/sources/memory_source.h"

namespace {

/// @brief Create a synthetic image for benchmarking
fim::MemorySource CreateSyntheticImage(int width, int height, int channels) {
  std::vector<uint8_t> data(width * height * channels);

  // Fill with gradient pattern
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      for (int c = 0; c < channels; ++c) {
        int idx = (y * width + x) * channels + c;
        data[idx] = static_cast<uint8_t>((x + y + c * 50) % 256);
      }
    }
  }

  fim::ImageInfo dims(width, height, channels, fim::PixelType::kUInt8,
                      fim::DataLayout::kChannelsLast);
  return fim::MemorySource::Create(std::move(data), dims);
}

}  // namespace

/// @brief Benchmark resize with different kernels
static void BM_Resize_Lanczos2(benchmark::State& state) {
  int input_size = state.range(0);
  int output_size = state.range(1);

  for (auto _ : state) {
    auto source = CreateSyntheticImage(input_size, input_size, 3);
    fim::Resize resize(std::move(source), output_size, output_size,
                       fim::resize::KernelType::kLanczos2);
    auto sink = fim::MemorySink::Create();
    sink.Render(resize);
    benchmark::DoNotOptimize(sink);
  }

  // Report throughput
  int64_t pixels_per_iter = static_cast<int64_t>(output_size) * output_size;
  state.SetItemsProcessed(state.iterations() * pixels_per_iter);
  state.SetBytesProcessed(state.iterations() * pixels_per_iter * 3);
}

static void BM_Resize_Lanczos3(benchmark::State& state) {
  int input_size = state.range(0);
  int output_size = state.range(1);

  for (auto _ : state) {
    auto source = CreateSyntheticImage(input_size, input_size, 3);
    fim::Resize resize(std::move(source), output_size, output_size,
                       fim::resize::KernelType::kLanczos3);
    auto sink = fim::MemorySink::Create();
    sink.Render(resize);
    benchmark::DoNotOptimize(sink);
  }

  int64_t pixels_per_iter = static_cast<int64_t>(output_size) * output_size;
  state.SetItemsProcessed(state.iterations() * pixels_per_iter);
  state.SetBytesProcessed(state.iterations() * pixels_per_iter * 3);
}

static void BM_Resize_Magic2021(benchmark::State& state) {
  int input_size = state.range(0);
  int output_size = state.range(1);

  for (auto _ : state) {
    auto source = CreateSyntheticImage(input_size, input_size, 3);
    fim::Resize resize(std::move(source), output_size, output_size,
                       fim::resize::KernelType::kMagic2021);
    auto sink = fim::MemorySink::Create();
    sink.Render(resize);
    benchmark::DoNotOptimize(sink);
  }

  int64_t pixels_per_iter = static_cast<int64_t>(output_size) * output_size;
  state.SetItemsProcessed(state.iterations() * pixels_per_iter);
  state.SetBytesProcessed(state.iterations() * pixels_per_iter * 3);
}

/// @brief Benchmark resize with box parameter
static void BM_Resize_WithBox(benchmark::State& state) {
  int input_size = state.range(0);
  int output_size = state.range(1);

  // Create box for subpixel region
  fim::resize::Box box(10.5F, 20.5F, static_cast<float>(input_size) - 10.5F,
                       static_cast<float>(input_size) - 20.5F);

  for (auto _ : state) {
    auto source = CreateSyntheticImage(input_size, input_size, 3);
    fim::Resize resize(std::move(source), output_size, output_size,
                       fim::resize::KernelType::kLanczos3, box);
    auto sink = fim::MemorySink::Create();
    sink.Render(resize);
    benchmark::DoNotOptimize(sink);
  }

  int64_t pixels_per_iter = static_cast<int64_t>(output_size) * output_size;
  state.SetItemsProcessed(state.iterations() * pixels_per_iter);
  state.SetBytesProcessed(state.iterations() * pixels_per_iter * 3);
}

/// @brief Benchmark resize with different channel counts
static void BM_Resize_Grayscale(benchmark::State& state) {
  int input_size = state.range(0);
  int output_size = state.range(1);

  for (auto _ : state) {
    auto source = CreateSyntheticImage(input_size, input_size, 1);
    fim::Resize resize(std::move(source), output_size, output_size,
                       fim::resize::KernelType::kLanczos3);
    auto sink = fim::MemorySink::Create();
    sink.Render(resize);
    benchmark::DoNotOptimize(sink);
  }

  int64_t pixels_per_iter = static_cast<int64_t>(output_size) * output_size;
  state.SetItemsProcessed(state.iterations() * pixels_per_iter);
  state.SetBytesProcessed(state.iterations() * pixels_per_iter * 1);
}

static void BM_Resize_RGBA(benchmark::State& state) {
  int input_size = state.range(0);
  int output_size = state.range(1);

  for (auto _ : state) {
    auto source = CreateSyntheticImage(input_size, input_size, 4);
    fim::Resize resize(std::move(source), output_size, output_size,
                       fim::resize::KernelType::kLanczos3);
    auto sink = fim::MemorySink::Create();
    sink.Render(resize);
    benchmark::DoNotOptimize(sink);
  }

  int64_t pixels_per_iter = static_cast<int64_t>(output_size) * output_size;
  state.SetItemsProcessed(state.iterations() * pixels_per_iter);
  state.SetBytesProcessed(state.iterations() * pixels_per_iter * 4);
}

// Register benchmarks with various input/output size combinations

// Downsampling benchmarks (2x, 4x)
BENCHMARK(BM_Resize_Lanczos2)
    ->Args({1024, 512})   // 2x downsample
    ->Args({1024, 256})   // 4x downsample
    ->Args({2048, 1024})  // 2x downsample, larger image
    ->Args({4096, 2048})  // 2x downsample, very large image
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Resize_Lanczos3)
    ->Args({1024, 512})
    ->Args({1024, 256})
    ->Args({2048, 1024})
    ->Args({4096, 2048})
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Resize_Magic2021)
    ->Args({1024, 512})
    ->Args({1024, 256})
    ->Args({2048, 1024})
    ->Args({4096, 2048})
    ->Unit(benchmark::kMillisecond);

// Upsampling benchmarks (2x, 4x)
BENCHMARK(BM_Resize_Lanczos3)
    ->Args({256, 512})   // 2x upsample
    ->Args({256, 1024})  // 4x upsample
    ->Args({512, 1024})  // 2x upsample
    ->Unit(benchmark::kMillisecond);

// Box parameter overhead
BENCHMARK(BM_Resize_WithBox)
    ->Args({1024, 512})
    ->Args({2048, 1024})
    ->Unit(benchmark::kMillisecond);

// Different channel counts
BENCHMARK(BM_Resize_Grayscale)
    ->Args({1024, 512})
    ->Args({2048, 1024})
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Resize_RGBA)
    ->Args({1024, 512})
    ->Args({2048, 1024})
    ->Unit(benchmark::kMillisecond);

// Non-uniform scaling
BENCHMARK(BM_Resize_Lanczos3)
    ->Args({1024, 768})   // 1024x1024 -> 768x768
    ->Args({1920, 1080})  // HD resolution resize
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();

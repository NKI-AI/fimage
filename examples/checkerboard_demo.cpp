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
#include <fim/operators/downsample.h>
#include <fim/sinks/memory_sink.h>
#include <fim/sinks/tiff_sink.h>
#include <fim/sources/memory_source.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "CLI11/CLI11.hpp"
#include "aifocore/utilities/thread_pool_singleton.h"

// Generate a checkerboard pattern in memory
std::vector<uint8_t> GenerateCheckerboard(int width, int height, int channels,
                                          int square_size = 100) {
  std::vector<uint8_t> data(width * height * channels);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // Determine if this square should be black or white
      int square_x = x / square_size;
      int square_y = y / square_size;
      bool is_white = (square_x + square_y) % 2 == 0;
      uint8_t value = is_white ? 255 : 0;

      // Set all channels to the same value
      int offset = (y * width + x) * channels;
      for (int c = 0; c < channels; ++c) {
        data[offset + c] = value;
      }
    }
  }

  return data;
}

int main(int argc, char* argv[]) {
  int image_size = 10000;
  int square_size = 100;
  bool pyramidal = false;
  bool benchmark = false;
  bool benchmark_memory = false;
  CLI::App app{
      "FImage Checkerboard Demo - Pure memory-to-TIFF performance test"};

  app.add_option("--size", image_size,
                 "Size of the square checkerboard image (default: 10000, "
                 "practical max: ~35000)")
      ->check(CLI::Range(100, 50000));

  app.add_option("--square-size", square_size,
                 "Size of each checkerboard square in pixels (default: 100)")
      ->check(CLI::Range(10, 1000));

  app.add_flag("--pyramidal", pyramidal,
               "Create a pyramidal TIFF with multiple levels");

  app.add_flag("--benchmark", benchmark,
               "Run benchmark comparing single vs multi-threaded performance");

  app.add_flag("--benchmark-memory", benchmark_memory,
               "Run benchmark for memory-to-memory operations (no file I/O)");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return app.exit(e);
  }

  try {
    // Check memory requirements
    size_t memory_bytes = static_cast<size_t>(image_size) * image_size * 3;
    double memory_gb = memory_bytes / (1024.0 * 1024.0 * 1024.0);

    if (memory_gb > 4.5) {
      std::cerr << "Warning: Requested image size requires " << memory_gb
                << " GB of memory.\n";
      std::cerr << "This may fail on systems with limited RAM.\n";
      std::cerr << "Consider using --size 35000 or smaller.\n\n";
    }

    std::cout << "=== FImage Checkerboard Demo ===\n";
    std::cout << "Generating " << image_size << "x" << image_size
              << " checkerboard pattern...\n";

    // Generate checkerboard pattern
    auto start_gen = std::chrono::high_resolution_clock::now();
    auto checkerboard_data =
        GenerateCheckerboard(image_size, image_size, 3, square_size);
    auto end_gen = std::chrono::high_resolution_clock::now();
    auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_gen - start_gen)
                            .count();

    std::cout << "Generated checkerboard in " << gen_duration << "ms\n";
    std::cout << "Memory size: "
              << (checkerboard_data.size() / (1024.0 * 1024.0)) << " MB\n\n";

    if (benchmark_memory) {
      std::cout << "=== Running Memory-to-Memory Benchmark ===\n";
      std::cout << "(Pure computational speedup - no file I/O)\n\n";

      // Note: For large images, only keep 2 copies in memory at once
      // First test uses the original, second test uses a copy

      // Test with 1 thread
      std::cout << "--- Single-threaded (NUM_THREADS=1) ---\n";
      aifocore::ThreadPoolManager::SetThreadCount(1);

      auto start_single = std::chrono::high_resolution_clock::now();
      {
        fim::MemorySource source = fim::MemorySource::Create(
            std::move(checkerboard_data),
            fim::ImageInfo(image_size, image_size, 3, fim::PixelType::kUInt8,
                           fim::DataLayout::kChannelsLast),
            fim::TileSize(256, 256));

        fim::MemorySink sink = fim::MemorySink::Create();
        fim::Image<fim::MemorySource>(std::move(source))
            .Crop(1000, 1000, 4000, 4000)
            .Downsample(2)
            .Render(sink);

        std::cout << "Output size: "
                  << (sink.GetDataAs<uint8_t>().size() / (1024.0 * 1024.0))
                  << " MB\n";
      }
      auto end_single = std::chrono::high_resolution_clock::now();
      auto single_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_single -
                                                                start_single)
              .count();

      std::cout << "Completed in " << single_duration << "ms\n\n";

      // Generate fresh data for second test (original was moved)
      std::cout << "Regenerating pattern for multi-threaded test...\n";
      auto checkerboard_data2 =
          GenerateCheckerboard(image_size, image_size, 3, square_size);

      // Test with 8 threads
      std::cout << "--- Multi-threaded (NUM_THREADS=8) ---\n";
      aifocore::ThreadPoolManager::SetThreadCount(8);

      auto start_multi = std::chrono::high_resolution_clock::now();
      {
        fim::MemorySource source = fim::MemorySource::Create(
            std::move(checkerboard_data2),
            fim::ImageInfo(image_size, image_size, 3, fim::PixelType::kUInt8,
                           fim::DataLayout::kChannelsLast),
            fim::TileSize(256, 256));

        fim::MemorySink sink = fim::MemorySink::Create();
        fim::Image<fim::MemorySource>(std::move(source))
            .Crop(1000, 1000, 4000, 4000)
            .Downsample(2)
            .Render(sink);

        std::cout << "Output size: "
                  << (sink.GetDataAs<uint8_t>().size() / (1024.0 * 1024.0))
                  << " MB\n";
      }
      auto end_multi = std::chrono::high_resolution_clock::now();
      auto multi_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_multi -
                                                                start_multi)
              .count();

      std::cout << "Completed in " << multi_duration << "ms\n\n";

      // Calculate speedup
      double speedup = static_cast<double>(single_duration) / multi_duration;
      std::cout << "=== Memory-to-Memory Benchmark Results ===\n";
      std::cout << "Single-threaded: " << single_duration << "ms\n";
      std::cout << "Multi-threaded:  " << multi_duration << "ms\n";
      std::cout << "Speedup:         " << speedup << "x\n";
      std::cout << "\nNote: This is pure computational speedup without any "
                   "file I/O overhead.\n";

    } else if (benchmark) {
      std::cout << "=== Running Benchmark ===\n\n";

      // Test with 1 thread
      std::cout << "--- Single-threaded (NUM_THREADS=1) ---\n";
      aifocore::ThreadPoolManager::SetThreadCount(1);

      auto start_single = std::chrono::high_resolution_clock::now();
      {
        fim::MemorySource source = fim::MemorySource::Create(
            std::vector<uint8_t>(checkerboard_data),
            fim::ImageInfo(image_size, image_size, 3, fim::PixelType::kUInt8,
                           fim::DataLayout::kChannelsLast),
            fim::TileSize(256, 256));

        fim::Image<fim::MemorySource>(std::move(source))
            .Crop(1000, 1000, 4000, 4000)
            .Downsample(2)
            .Render(fim::TiffSink::Create("checkerboard_single.tiff",
                                          fim::TileSize(512, 512), pyramidal,
                                          10, 2));
      }
      auto end_single = std::chrono::high_resolution_clock::now();
      auto single_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_single -
                                                                start_single)
              .count();

      std::cout << "Completed in " << single_duration << "ms\n";
      std::cout << "Output: checkerboard_single.tiff\n\n";

      // Test with 8 threads
      std::cout << "--- Multi-threaded (NUM_THREADS=8) ---\n";
      aifocore::ThreadPoolManager::SetThreadCount(8);

      auto start_multi = std::chrono::high_resolution_clock::now();
      {
        fim::MemorySource source = fim::MemorySource::Create(
            std::vector<uint8_t>(checkerboard_data),
            fim::ImageInfo(image_size, image_size, 3, fim::PixelType::kUInt8,
                           fim::DataLayout::kChannelsLast),
            fim::TileSize(256, 256));

        fim::Image<fim::MemorySource>(std::move(source))
            .Crop(1000, 1000, 4000, 4000)
            .Downsample(2)
            .Render(fim::TiffSink::Create("checkerboard_multi.tiff",
                                          fim::TileSize(512, 512), pyramidal,
                                          10, 2));
      }
      auto end_multi = std::chrono::high_resolution_clock::now();
      auto multi_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_multi -
                                                                start_multi)
              .count();

      std::cout << "Completed in " << multi_duration << "ms\n";
      std::cout << "Output: checkerboard_multi.tiff\n\n";

      // Calculate speedup
      double speedup = static_cast<double>(single_duration) / multi_duration;
      std::cout << "=== Benchmark Results ===\n";
      std::cout << "Single-threaded: " << single_duration << "ms\n";
      std::cout << "Multi-threaded:  " << multi_duration << "ms\n";
      std::cout << "Speedup:         " << speedup << "x\n";

    } else {
      // Regular mode (use current thread count from environment)
      auto& pool = aifocore::ThreadPoolManager::GetInstance();
      std::cout << "Using " << pool.get_thread_count() << " thread(s)\n";

      if (pyramidal) {
        std::cout << "\n=== Creating PYRAMIDAL TIFF ===\n";
        std::cout << "This will create a multi-page TIFF with progressively "
                     "smaller levels\n";
        std::cout << "until dimensions are smaller than the tile size.\n\n";

        auto start = std::chrono::high_resolution_clock::now();
        {
          fim::MemorySource source = fim::MemorySource::Create(
              std::move(checkerboard_data),
              fim::ImageInfo(image_size, image_size, 3, fim::PixelType::kUInt8,
                             fim::DataLayout::kChannelsLast),
              fim::TileSize(256, 256));

          fim::Image<fim::MemorySource>(std::move(source))
              .Crop(1000, 1000, 4000, 4000)
              .Downsample(2)
              .Render(fim::TiffSink::Create("checkerboard_pyramidal.tiff",
                                            fim::TileSize(512, 512), true, 10,
                                            2));
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();

        std::cout << "Pyramidal TIFF created in " << duration << "ms\n";
        std::cout << "Output: checkerboard_pyramidal.tiff\n";
        std::cout << "Use 'tiffinfo checkerboard_pyramidal.tiff' to inspect "
                     "the pyramid levels.\n";

      } else {
        std::cout << "\n=== Creating SINGLE-PAGE TIFF ===\n";
        std::cout << "This will create a standard single-page TIFF file.\n\n";

        auto start = std::chrono::high_resolution_clock::now();
        {
          fim::MemorySource source = fim::MemorySource::Create(
              std::move(checkerboard_data),
              fim::ImageInfo(image_size, image_size, 3, fim::PixelType::kUInt8,
                             fim::DataLayout::kChannelsLast),
              fim::TileSize(256, 256));

          fim::Image<fim::MemorySource>(std::move(source))
              .Crop(1000, 1000, 4000, 4000)
              .Downsample(2)
              .Render(fim::TiffSink::Create("checkerboard_single.tiff",
                                            fim::TileSize(512, 512)));
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();

        std::cout << "Single-page TIFF created in " << duration << "ms\n";
        std::cout << "Output: checkerboard_single.tiff\n";
        std::cout
            << "Use 'tiffinfo checkerboard_single.tiff' to inspect the file.\n";
      }
    }

    std::cout << "\n=== Demo completed successfully! ===\n";
    std::cout << "Try running with different options:\n";
    std::cout << "  " << argv[0]
              << " --benchmark               # Compare single vs "
                 "multi-threaded (TIFF)\n";
    std::cout
        << "  " << argv[0]
        << " --benchmark-memory        # Compare memory-to-memory (no I/O)\n";
    std::cout << "  " << argv[0]
              << " --size 20000             # Larger image\n";
    std::cout << "  " << argv[0]
              << " --pyramidal              # Create pyramidal TIFF\n";
    std::cout << "  " << argv[0]
              << " --square-size 50         # Smaller checkerboard squares\n";

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

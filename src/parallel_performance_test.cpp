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
 * @file parallel_performance_test.cpp
 * @brief Multi-threaded performance and correctness tests for FImage I/O.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file tests the parallel performance improvements made to FImageSource
 * and FImageSink, including lock-free BufferPool, reduced critical sections,
 * and parallel compression.
 */
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#include "fim/sinks/fimage_sink.h"
#include "fim/sources/fimage_source.h"
#include "fim/sources/memory_source.h"
#include "fim/utilities/buffer_pool.h"

namespace fim {
namespace {

// Helper to create test image data with a pattern
std::vector<uint8_t> CreatePatternImageData(int width, int height,
                                            int channels) {
  std::vector<uint8_t> data(width * height * channels);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      for (int c = 0; c < channels; ++c) {
        int idx = (y * width + x) * channels + c;
        data[idx] = static_cast<uint8_t>((x + y + c) % 256);
      }
    }
  }
  return data;
}

// Helper to compare image data
bool CompareImageData(const std::vector<uint8_t>& a,
                      const std::vector<uint8_t>& b) {
  if (a.size() != b.size())
    return false;
  return std::equal(a.begin(), a.end(), b.begin());
}

// Test fixture
class ParallelPerformanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = fs::temp_directory_path() / "fimage_parallel_test";
    fs::create_directories(test_dir_);
  }

  void TearDown() override {
    if (fs::exists(test_dir_)) {
      fs::remove_all(test_dir_);
    }
  }

  fs::path GetTestPath(const std::string& name) const {
    return test_dir_ / name;
  }

  fs::path test_dir_;
};

// ============================================================================
// BufferPool Lock-Free Tests
// ============================================================================

TEST_F(ParallelPerformanceTest, BufferPoolConcurrentAccess) {
  BufferPool pool;
  constexpr int kNumThreads = 8;
  constexpr int kNumOpsPerThread = 1000;
  constexpr size_t kBufferSize = 4096;

  std::vector<std::thread> threads;
  std::atomic<int> error_count{0};

  // Launch threads that concurrently get and return buffers
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&pool, &error_count]() {
      try {
        for (int i = 0; i < kNumOpsPerThread; ++i) {
          // Get a buffer
          auto buffer = pool.GetBuffer(kBufferSize);

          // Verify buffer size
          if (buffer.size() != kBufferSize) {
            error_count.fetch_add(1);
          }

          // Write pattern to buffer
          for (size_t j = 0; j < buffer.size(); ++j) {
            buffer[j] = static_cast<uint8_t>(j % 256);
          }

          // Return buffer to pool
          pool.ReturnBuffer(std::move(buffer));
        }
      } catch (...) {
        error_count.fetch_add(1);
      }
    });
  }

  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }

  // No errors should have occurred
  EXPECT_EQ(error_count.load(), 0);
}

TEST_F(ParallelPerformanceTest, BufferPoolNoMemoryLeaks) {
  BufferPool pool;
  constexpr int kNumBuffers = 100;
  constexpr size_t kBufferSize = 8192;

  // Get and return many buffers
  for (int i = 0; i < kNumBuffers; ++i) {
    auto buffer = pool.GetBuffer(kBufferSize);
    pool.ReturnBuffer(std::move(buffer));
  }

  // Pool should have cached some buffers (up to max pool size)
  size_t pool_size = pool.Size();
  EXPECT_GT(pool_size, 0);
  EXPECT_LE(pool_size, 64);  // kMaxPoolSize

  // Clear should free all buffers
  pool.Clear();
  EXPECT_EQ(pool.Size(), 0);
}

// ============================================================================
// FImageSource Multi-threaded Read Tests
// ============================================================================

TEST_F(ParallelPerformanceTest, SourceConcurrentTileReads) {
  // Create test image
  int width = 512;
  int height = 512;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write compressed tiled file
  auto path = GetTestPath("concurrent_reads.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(64, 64));
  auto sink = FImageSink::Create(path, TileSize(64, 64), CompressionType::kLZ4);
  sink.Render(source);

  // Read concurrently from multiple threads
  auto fimage_source = FImageSource::Create(path);

  constexpr int kNumThreads = 8;
  std::vector<std::thread> threads;
  std::atomic<int> error_count{0};

  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&fimage_source, &original_data, width, height,
                          channels, &error_count]() {
      try {
        // Each thread reads different regions
        for (int i = 0; i < 10; ++i) {
          int tile_x = (i * 64) % (width - 64);
          int tile_y = (i * 32) % (height - 64);

          auto tile = fimage_source.GetTile(tile_x, tile_y, 64, 64);

          // Verify data matches original
          for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
              for (int c = 0; c < channels; ++c) {
                int tile_idx = (y * 64 + x) * channels + c;
                int orig_idx =
                    ((tile_y + y) * width + (tile_x + x)) * channels + c;
                if (tile.GetData()[tile_idx] != original_data[orig_idx]) {
                  error_count.fetch_add(1);
                  return;
                }
              }
            }
          }
        }
      } catch (...) {
        error_count.fetch_add(1);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(error_count.load(), 0);
}

TEST_F(ParallelPerformanceTest, SourceStressTestDecompression) {
  // Create large compressed tiled file
  int width = 1024;
  int height = 1024;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  auto path = GetTestPath("stress_decompress.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(128, 128));
  auto sink =
      FImageSink::Create(path, TileSize(128, 128), CompressionType::kZstd);
  sink.Render(source);

  // Read many tiles concurrently (stresses decompression parallelization)
  auto fimage_source = FImageSource::Create(path);

  constexpr int kNumThreads = 16;
  std::vector<std::thread> threads;
  std::atomic<int> tiles_read{0};
  std::atomic<int> error_count{0};

  auto start_time = std::chrono::high_resolution_clock::now();

  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back(
        [&fimage_source, width, height, &tiles_read, &error_count, t]() {
          try {
            // Each thread reads different tiles
            for (int i = t; i < 64; i += kNumThreads) {
              int tile_x = ((i % 8) * 128) % (width - 128);
              int tile_y = ((i / 8) * 128) % (height - 128);

              auto tile = fimage_source.GetTile(tile_x, tile_y, 128, 128);

              // Verify we got correct size
              if (tile.width != 128 || tile.height != 128) {
                error_count.fetch_add(1);
                return;
              }

              tiles_read.fetch_add(1);
            }
          } catch (...) {
            error_count.fetch_add(1);
          }
        });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_EQ(error_count.load(), 0);
  EXPECT_EQ(tiles_read.load(), 64);

  // Log performance (informational, not enforced)
  std::cout << "Parallel decompression: " << tiles_read.load() << " tiles in "
            << duration.count() << "ms" << std::endl;
}

// ============================================================================
// FImageSink Multi-threaded Write Tests
// ============================================================================

TEST_F(ParallelPerformanceTest, SinkParallelCompressionCorrectness) {
  // Create test image
  int width = 512;
  int height = 512;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  // Write with parallel compression
  auto path = GetTestPath("parallel_compress.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(64, 64));

  // Use large tile count to ensure parallelization kicks in
  auto sink = FImageSink::Create(path, TileSize(64, 64), CompressionType::kLZ4);
  sink.Render(source);

  // Read back and verify correctness
  auto fimage_source = FImageSource::Create(path);
  auto tile = fimage_source.GetTile(0, 0, width, height);

  EXPECT_TRUE(CompareImageData(tile.GetData(), original_data));
}

TEST_F(ParallelPerformanceTest, SinkParallelCompressionLargeImage) {
  // Create large image to ensure parallel path is taken
  int width = 2048;
  int height = 2048;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  auto path = GetTestPath("large_parallel.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(256, 256));

  auto start_time = std::chrono::high_resolution_clock::now();

  auto sink =
      FImageSink::Create(path, TileSize(256, 256), CompressionType::kZstd);
  sink.Render(source);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  // Verify correctness
  auto fimage_source = FImageSource::Create(path);

  // Sample-based verification (full verification would be slow)
  std::vector<std::pair<int, int>> sample_points = {
      {0, 0}, {256, 256}, {512, 512}, {1024, 1024}, {1792, 1792}};

  for (const auto& [x, y] : sample_points) {
    auto tile = fimage_source.GetTile(x, y, 64, 64);

    // Verify sample data
    for (int ty = 0; ty < 64; ++ty) {
      for (int tx = 0; tx < 64; ++tx) {
        for (int c = 0; c < channels; ++c) {
          int tile_idx = (ty * 64 + tx) * channels + c;
          int orig_idx = ((y + ty) * width + (x + tx)) * channels + c;
          EXPECT_EQ(tile.GetData()[tile_idx], original_data[orig_idx]);
        }
      }
    }
  }

  std::cout << "Parallel compression of " << width << "x" << height
            << " image: " << duration.count() << "ms" << std::endl;
}

TEST_F(ParallelPerformanceTest, SinkSeekTableCorrectness) {
  // Verify that parallel compression maintains correct seek table ordering
  int width = 512;
  int height = 512;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  auto path = GetTestPath("seek_table.fimage");
  auto source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(64, 64));
  auto sink = FImageSink::Create(path, TileSize(64, 64), CompressionType::kLZ4);
  sink.Render(source);

  // Read every tile in order and verify
  auto fimage_source = FImageSource::Create(path);

  for (int tile_y = 0; tile_y < 8; ++tile_y) {
    for (int tile_x = 0; tile_x < 8; ++tile_x) {
      int x = tile_x * 64;
      int y = tile_y * 64;

      auto tile = fimage_source.GetTile(x, y, 64, 64);

      // Verify data
      for (int ty = 0; ty < 64; ++ty) {
        for (int tx = 0; tx < 64; ++tx) {
          for (int c = 0; c < channels; ++c) {
            int tile_idx = (ty * 64 + tx) * channels + c;
            int orig_idx = ((y + ty) * width + (x + tx)) * channels + c;
            EXPECT_EQ(tile.GetData()[tile_idx], original_data[orig_idx]);
          }
        }
      }
    }
  }
}

// ============================================================================
// End-to-End Multi-threaded Pipeline Tests
// ============================================================================

TEST_F(ParallelPerformanceTest, EndToEndParallelPipeline) {
  // Test full pipeline: parallel write, then parallel read
  int width = 1024;
  int height = 1024;
  int channels = 3;
  auto original_data = CreatePatternImageData(width, height, channels);

  auto path = GetTestPath("end_to_end.fimage");

  // Parallel write
  auto write_source =
      MemorySource::Create(std::vector<uint8_t>(original_data),
                           ImageInfo(width, height, channels, PixelType::kUInt8,
                                     DataLayout::kChannelsLast),
                           TileSize(128, 128));
  auto sink =
      FImageSink::Create(path, TileSize(128, 128), CompressionType::kZstd);
  sink.Render(write_source);

  // Parallel read from multiple threads
  auto fimage_source = FImageSource::Create(path);

  constexpr int kNumThreads = 8;
  std::vector<std::thread> threads;
  std::atomic<int> error_count{0};

  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&fimage_source, &original_data, width, height,
                          channels, &error_count, t]() {
      try {
        // Each thread reads different tiles (distributed across the image)
        // Thread t reads tiles at positions (t*128, 0), (t*128, 128), etc.
        int base_x = (t % 8) * 128;  // 8 columns of 128-pixel tiles
        int base_y = 0;

        // Read a few tiles vertically
        for (int tile_idx = 0; tile_idx < 4; ++tile_idx) {
          int tile_y = base_y + tile_idx * 128;
          if (tile_y + 128 > height)
            break;

          auto tile = fimage_source.GetTile(base_x, tile_y, 128, 128);

          // Verify sample points in this tile
          for (int i = 0; i < 25; ++i) {
            int x = i % 25;
            int y = i / 25;

            for (int c = 0; c < channels; ++c) {
              int tile_idx_data = (y * 128 + x) * channels + c;
              int orig_idx =
                  ((tile_y + y) * width + (base_x + x)) * channels + c;
              if (tile.GetData()[tile_idx_data] != original_data[orig_idx]) {
                error_count.fetch_add(1);
                return;
              }
            }
          }
        }
      } catch (...) {
        error_count.fetch_add(1);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(error_count.load(), 0);
}

}  // namespace
}  // namespace fim

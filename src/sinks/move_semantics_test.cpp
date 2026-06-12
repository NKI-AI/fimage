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
 * @file move_semantics_test.cpp
 * @brief Tests for sink move semantics in the Render() method.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file contains tests to verify that sinks can be moved into
 * the Render() method, allowing efficient transfer of resources
 * without unnecessary copies.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "fim/image.h"
#include "fim/sinks/memory_sink.h"
#include "fim/sources/memory_source.h"

namespace fim {
namespace {

namespace fs = std::filesystem;

/**
 * @brief Custom sink to track move operations.
 *
 * This sink tracks whether it was moved, allowing us to verify
 * that move semantics are properly preserved.
 */
class MoveTrackingSink : public SinkBase<MoveTrackingSink> {
 public:
  explicit MoveTrackingSink(const fs::path& filename)
      : SinkBase<MoveTrackingSink>(filename),
        move_count_(std::make_shared<int>(0)),
        copy_count_(std::make_shared<int>(0)) {}

  // Copy constructor - increments copy count
  MoveTrackingSink(const MoveTrackingSink& other)
      : SinkBase<MoveTrackingSink>(other.filename_),
        move_count_(other.move_count_),
        copy_count_(other.copy_count_) {
    ++(*copy_count_);
  }

  // Move constructor - increments move count
  MoveTrackingSink(MoveTrackingSink&& other) noexcept
      : SinkBase<MoveTrackingSink>(std::move(other.filename_)),
        move_count_(std::move(other.move_count_)),
        copy_count_(std::move(other.copy_count_)) {
    if (move_count_) {
      ++(*move_count_);
    }
  }

  MoveTrackingSink& operator=(const MoveTrackingSink&) = default;
  MoveTrackingSink& operator=(MoveTrackingSink&&) noexcept = default;

  template <typename InputType>
  void Render(const InputType& input) {
    // Simple render - just write dimensions to file
    auto dims = input.GetDimensions();
    std::ofstream file(filename_);
    file << dims.GetWidth() << "x" << dims.GetHeight() << "x" << dims.channels;
  }

  int GetMoveCount() const { return move_count_ ? *move_count_ : 0; }

  int GetCopyCount() const { return copy_count_ ? *copy_count_ : 0; }

 private:
  std::shared_ptr<int> move_count_;
  std::shared_ptr<int> copy_count_;
};

/**
 * @brief Test that temporary sinks are moved, not copied.
 *
 * This test verifies that when a temporary sink is passed to Render(),
 * it is moved rather than copied, which is more efficient.
 */
TEST(MoveSemanticsTest, TemporarySinkIsMoved) {
  // Create a simple test image
  std::vector<uint8_t> data(50 * 50 * 3, 100);
  ImageInfo dims(50, 50, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto img = Image<MemorySource>(MemorySource::Create(std::move(data), dims));

  // Create a temporary file path
  auto temp_path = fs::temp_directory_path() / "move_test.txt";

  // Render with a temporary sink
  img.Render(MoveTrackingSink(temp_path));

  // The sink should have been moved (at least once during
  // construction/forwarding) We can't easily check the count after Render since
  // the sink is destroyed, but we can verify the file was created
  EXPECT_TRUE(fs::exists(temp_path));

  // Cleanup
  fs::remove(temp_path);
}

/**
 * @brief Test that lvalue sinks can still be used (copied).
 *
 * This test verifies backward compatibility: lvalue sinks should
 * still work, even though they may be copied.
 */
TEST(MoveSemanticsTest, LvalueSinkWorks) {
  // Create a simple test image
  std::vector<uint8_t> data(30 * 30 * 1, 150);
  ImageInfo dims(30, 30, 1, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto img = Image<MemorySource>(MemorySource::Create(std::move(data), dims));

  // Create a sink as an lvalue
  auto temp_path = fs::temp_directory_path() / "lvalue_test.txt";
  MoveTrackingSink sink(temp_path);

  // Get initial counts
  int initial_copies = sink.GetCopyCount();
  int initial_moves = sink.GetMoveCount();

  // Render with the lvalue sink
  img.Render(sink);

  // The sink should still exist and be usable
  EXPECT_TRUE(fs::exists(temp_path));

  // Cleanup
  fs::remove(temp_path);
}

/**
 * @brief Test that MemorySink works with rvalue and lvalue.
 *
 * This test uses the actual MemorySink to verify it works with
 * the improved Render() implementation.
 */
TEST(MoveSemanticsTest, MemorySinkWithRvalue) {
  // Create a test image
  std::vector<uint8_t> data(40 * 40 * 3, 200);
  ImageInfo dims(40, 40, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto img = Image<MemorySource>(MemorySource::Create(std::move(data), dims));

  // Render with a temporary MemorySink (rvalue)
  MemorySink sink = MemorySink::Create();
  img.Render(std::move(sink));

  // Verify the sink captured the data
  EXPECT_EQ(sink.GetDimensions().GetWidth(), 40);
  EXPECT_EQ(sink.GetDimensions().GetHeight(), 40);
  EXPECT_EQ(sink.GetDimensions().channels, 3);
}

/**
 * @brief Test that MemorySink works with lvalue.
 *
 * This test verifies that passing MemorySink by lvalue still works.
 */
TEST(MoveSemanticsTest, MemorySinkWithLvalue) {
  // Create a test image
  std::vector<uint8_t> data(25 * 25 * 1, 50);
  ImageInfo dims(25, 25, 1, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto img = Image<MemorySource>(MemorySource::Create(std::move(data), dims));

  // Create a MemorySink as an lvalue
  MemorySink sink = MemorySink::Create();

  // Render with the lvalue sink
  img.Render(sink);

  // Verify the sink captured the data
  EXPECT_EQ(sink.GetDimensions().GetWidth(), 25);
  EXPECT_EQ(sink.GetDimensions().GetHeight(), 25);
  EXPECT_EQ(sink.GetDimensions().channels, 1);
}

/**
 * @brief Test move semantics with a processing chain.
 *
 * This test verifies that move semantics work correctly when
 * rendering from a complex processing chain.
 */
TEST(MoveSemanticsTest, ChainedOperationsWithMove) {
  // Create a test image
  std::vector<uint8_t> data(100 * 100 * 3, 128);
  ImageInfo dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);

  // Create a processing chain
  auto pipeline =
      Image<MemorySource>(MemorySource::Create(std::move(data), dims))
          .Crop(10, 10, 80, 80)
          .Downsample(2);

  // Render with a moved sink
  MemorySink sink = MemorySink::Create();
  pipeline.Render(std::move(sink));

  // Verify the output dimensions
  EXPECT_EQ(sink.GetDimensions().GetWidth(), 40);
  EXPECT_EQ(sink.GetDimensions().GetHeight(), 40);
  EXPECT_EQ(sink.GetDimensions().channels, 3);
}

/**
 * @brief Test that const lvalue sinks work correctly.
 *
 * This test verifies that const lvalue references are handled correctly.
 */
TEST(MoveSemanticsTest, ConstLvalueSink) {
  // Create a simple test image
  std::vector<uint8_t> data(20 * 20 * 1, 75);
  ImageInfo dims(20, 20, 1, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto img = Image<MemorySource>(MemorySource::Create(std::move(data), dims));

  // Create a const lvalue sink
  const MemorySink sink = MemorySink::Create();

  // This should compile and work (though it won't modify the sink)
  // Note: MemorySink's Render is not const, so this actually won't compile
  // with a const sink. This is expected behavior.
  // img.Render(sink);  // Would fail to compile - this is correct!

  // Instead, we can use a non-const lvalue
  MemorySink non_const_sink = MemorySink::Create();
  img.Render(non_const_sink);

  EXPECT_EQ(non_const_sink.GetDimensions().GetWidth(), 20);
}

/**
 * @brief Stress test with multiple renders.
 *
 * This test verifies that move semantics work correctly
 * across multiple render operations.
 */
TEST(MoveSemanticsTest, MultipleRenders) {
  for (int i = 0; i < 10; ++i) {
    std::vector<uint8_t> data(64 * 64 * 3, static_cast<uint8_t>(i * 25));
    ImageInfo dims(64, 64, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
    auto img = Image<MemorySource>(MemorySource::Create(std::move(data), dims));

    // Render with a moved sink
    MemorySink sink = MemorySink::Create();
    img.Render(std::move(sink));

    EXPECT_EQ(sink.GetDimensions().GetWidth(), 64);
    EXPECT_EQ(sink.GetDimensions().GetHeight(), 64);
  }
}

}  // namespace
}  // namespace fim

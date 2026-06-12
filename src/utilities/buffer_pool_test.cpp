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
 * @file buffer_pool_test.cpp
 * @brief Tests for BufferPool lock-free implementation.
 * @author Jonas Teuwen
 * @date 2025
 */
#include "fim/utilities/buffer_pool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

namespace fim {

class BufferPoolTest : public ::testing::Test {
 protected:
  void SetUp() override { pool_ = std::make_unique<BufferPool<>>(); }

  void TearDown() override { pool_.reset(); }

  std::unique_ptr<BufferPool<>> pool_;
};

// Basic test: get and return buffer
TEST_F(BufferPoolTest, BasicGetReturn) {
  auto buffer = pool_->GetBuffer(1024);
  EXPECT_GE(buffer.size(), 1024);

  // Return it
  pool_->ReturnBuffer(std::move(buffer));
  EXPECT_EQ(pool_->Size(), 1);
}

// Test: get buffer when pool is empty
TEST_F(BufferPoolTest, GetBufferEmptyPool) {
  EXPECT_EQ(pool_->Size(), 0);

  auto buffer = pool_->GetBuffer(512);
  EXPECT_GE(buffer.size(), 512);
  EXPECT_EQ(pool_->Size(), 0);  // Still empty
}

// Test: get buffer from non-empty pool
TEST_F(BufferPoolTest, GetBufferFromPool) {
  // Pre-populate pool
  pool_->ReturnBuffer(std::vector<uint8_t>(1024));
  EXPECT_EQ(pool_->Size(), 1);

  // Get buffer
  auto buffer = pool_->GetBuffer(512);
  EXPECT_GE(buffer.size(), 512);
  EXPECT_EQ(pool_->Size(), 0);  // Now empty
}

// Test: multiple get/return operations
TEST_F(BufferPoolTest, MultipleGetReturn) {
  for (int i = 0; i < 10; ++i) {
    auto buffer = pool_->GetBuffer(256);
    pool_->ReturnBuffer(std::move(buffer));
  }

  // Pool should have buffers (up to max size limit)
  EXPECT_GT(pool_->Size(), 0);
}

// Test: pool size limit
TEST_F(BufferPoolTest, PoolSizeLimit) {
  // Try to add more than max pool size (64)
  for (int i = 0; i < 100; ++i) {
    pool_->ReturnBuffer(std::vector<uint8_t>(128));
  }

  // Pool should not exceed max size
  EXPECT_LE(pool_->Size(), 64);
}

// Test: Clear operation
TEST_F(BufferPoolTest, Clear) {
  // Add some buffers
  for (int i = 0; i < 10; ++i) {
    pool_->ReturnBuffer(std::vector<uint8_t>(256));
  }

  size_t size_before = pool_->Size();
  EXPECT_GT(size_before, 0);

  // Clear
  pool_->Clear();
  EXPECT_EQ(pool_->Size(), 0);
}

// Stress test: high thread contention
// This test specifically targets the ABA problem
TEST_F(BufferPoolTest, HighContentionStressTest) {
  constexpr int kNumThreads = 16;
  constexpr int kOperationsPerThread = 1000;

  std::vector<std::thread> threads;
  std::atomic<int> completed_threads{0};

  // Each thread repeatedly gets and returns buffers
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([this, &completed_threads]() {
      for (int i = 0; i < kOperationsPerThread; ++i) {
        // Get buffer
        auto buffer = pool_->GetBuffer(512 + (i % 10) * 64);

        // Do some minimal work
        buffer[0] = static_cast<uint8_t>(i % 256);

        // Return buffer
        pool_->ReturnBuffer(std::move(buffer));
      }
      completed_threads.fetch_add(1, std::memory_order_relaxed);
    });
  }

  // Wait for all threads
  for (auto& thread : threads) {
    thread.join();
  }

  // Verify all threads completed
  EXPECT_EQ(completed_threads.load(), kNumThreads);

  // Pool should have some buffers (within limits)
  EXPECT_LE(pool_->Size(), 64);
}

// Stress test: ABA scenario simulation
// Multiple threads popping and pushing simultaneously to trigger potential ABA
TEST_F(BufferPoolTest, ABAStressTest) {
  constexpr int kNumThreads = 20;
  constexpr int kIterations = 500;

  // Pre-populate pool with some buffers
  for (int i = 0; i < 30; ++i) {
    pool_->ReturnBuffer(std::vector<uint8_t>(1024));
  }

  std::vector<std::thread> threads;
  std::atomic<bool> start_flag{false};
  std::atomic<int> operation_count{0};

  // Launch threads that will start simultaneously
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([this, &start_flag, &operation_count]() {
      // Wait for start signal
      while (!start_flag.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }

      // Rapid get/return to maximize contention
      for (int i = 0; i < kIterations; ++i) {
        auto buffer = pool_->GetBuffer(1024);
        operation_count.fetch_add(1, std::memory_order_relaxed);

        // Immediate return to maximize ABA potential
        pool_->ReturnBuffer(std::move(buffer));
        operation_count.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  // Start all threads simultaneously
  start_flag.store(true, std::memory_order_release);

  // Wait for completion
  for (auto& thread : threads) {
    thread.join();
  }

  // Verify all operations completed
  int expected_ops = kNumThreads * kIterations * 2;  // get + return
  EXPECT_EQ(operation_count.load(), expected_ops);
}

// Test: deferred reclamation behavior
TEST_F(BufferPoolTest, DeferredReclamation) {
  constexpr int kNumBuffers = 100;

  // Get and immediately return many buffers to trigger cleanup
  for (int i = 0; i < kNumBuffers; ++i) {
    auto buffer = pool_->GetBuffer(256);

    // Add some to pool first to populate it
    if (i < 30) {
      pool_->ReturnBuffer(std::move(buffer));
    }
  }

  // Now do rapid get/return to trigger deferred reclamation cleanup
  for (int i = 0; i < 50; ++i) {
    auto buffer = pool_->GetBuffer(256);
    pool_->ReturnBuffer(std::move(buffer));
  }

  // Pool should still be functional and within size limits
  EXPECT_LE(pool_->Size(), 64);

  // Should still be able to get buffers
  auto buffer = pool_->GetBuffer(512);
  EXPECT_GE(buffer.size(), 512);
}

// Test: concurrent size queries
TEST_F(BufferPoolTest, ConcurrentSizeQueries) {
  constexpr int kNumThreads = 8;
  constexpr int kIterations = 100;

  std::vector<std::thread> threads;

  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([this]() {
      for (int i = 0; i < kIterations; ++i) {
        // Get buffer
        auto buffer = pool_->GetBuffer(256);

        // Query size (should not crash)
        size_t size = pool_->Size();
        EXPECT_GE(size, 0);  // Size should be non-negative

        // Return buffer
        pool_->ReturnBuffer(std::move(buffer));

        // Query size again
        size = pool_->Size();
        EXPECT_GE(size, 0);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

}  // namespace fim

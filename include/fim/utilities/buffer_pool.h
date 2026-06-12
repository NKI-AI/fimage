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
 * @file buffer_pool.h
 * @brief Lock-free thread-safe typed buffer pool for reusing allocations.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file defines a BufferPool template class that provides lock-free
 * thread-safe buffer reuse for any type. It uses a Treiber stack (lock-free
 * stack) for concurrent access without mutex contention.
 */
#ifndef AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_BUFFER_POOL_H_
#define AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_BUFFER_POOL_H_

#include <atomic>
#include <cstddef>
#include <vector>

#include "aifocore/utilities/treiber_stack.h"

namespace fim {

/**
 * @brief Lock-free thread-safe buffer pool for reusing typed buffers.
 *
 * This class provides a lock-free buffer pool that reuses std::vector<T>
 * allocations to reduce allocation overhead. The pool uses a Treiber stack
 * (lock-free stack) for concurrent access without mutex contention.
 *
 * Buffers are resized as needed, and the pool retains buffers for future use.
 * This is particularly beneficial when reading many tiles of similar sizes
 * in a multi-threaded environment.
 *
 * The lock-free design eliminates mutex bottlenecks, allowing concurrent
 * operations to acquire and return buffers without serialization.
 *
 * ABA Prevention: The pool uses deferred reclamation to prevent ABA problems.
 * Nodes are not deleted immediately after popping; instead, they are moved to
 * a retired list and deleted later when it's safe.
 *
 * @tparam T The element type for the buffer vectors (defaults to uint8_t for
 * backwards compatibility)
 */
template <typename T = uint8_t>
class BufferPool {
 public:
  /**
   * @brief Constructs an empty buffer pool.
   */
  BufferPool() : pool_size_(0), retired_count_(0) {}

  /**
   * @brief Destructor that frees all cached buffers.
   */
  ~BufferPool() { Clear(); }

  /**
   * @brief Deleted copy constructor.
   */
  BufferPool(const BufferPool&) = delete;

  /**
   * @brief Deleted copy assignment.
   */
  BufferPool& operator=(const BufferPool&) = delete;

  /**
   * @brief Gets a buffer from the pool or creates a new one.
   *
   * If a buffer is available in the pool, it is returned (potentially resized).
   * Otherwise, a new buffer is allocated. The returned buffer is guaranteed to
   * have at least `min_size` capacity.
   *
   * This operation is lock-free and uses compare-and-swap to pop buffers
   * from the internal Treiber stack.
   *
   * @param min_size Minimum size required for the buffer
   * @return A buffer with at least `min_size` capacity
   */
  std::vector<T> GetBuffer(size_t min_size) {
    // Try to pop a buffer from the lock-free stack
    BufferNode* node = active_pool_.Pop();

    if (node != nullptr) {
      // Successfully got a node from pool
      std::vector<T> buffer = std::move(node->buffer);

      // Defer deletion to prevent ABA problem
      RetireNode(node);

      // Decrement pool size
      pool_size_.fetch_sub(1, std::memory_order_relaxed);

      // Resize if needed
      if (buffer.capacity() < min_size) {
        buffer.reserve(min_size);
      }
      buffer.resize(min_size);
      return buffer;
    }

    // Pool is empty, allocate a new buffer
    std::vector<T> buffer;
    buffer.reserve(min_size);
    buffer.resize(min_size);
    return buffer;
  }

  /**
   * @brief Returns a buffer to the pool for reuse.
   *
   * The buffer is moved into the pool's storage. To prevent unbounded memory
   * growth, the pool has a maximum capacity and will discard buffers if full.
   *
   * This operation is lock-free and uses compare-and-swap to push buffers
   * onto the internal Treiber stack.
   *
   * @param buffer The buffer to return to the pool (moved)
   */
  void ReturnBuffer(std::vector<T>&& buffer) {
    // Only keep buffers if we haven't exceeded the pool size limit
    constexpr size_t kMaxPoolSize = 64;  // Maximum number of cached buffers

    size_t current_size = pool_size_.load(std::memory_order_relaxed);
    if (current_size >= kMaxPoolSize) {
      // Pool is full, let the buffer be destroyed
      return;
    }

    // Create a new node
    BufferNode* new_node = new BufferNode;
    new_node->buffer = std::move(buffer);

    // Push onto the stack
    active_pool_.Push(new_node);

    // Increment pool size
    pool_size_.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * @brief Clears all buffers from the pool.
   *
   * This releases all cached buffers, freeing their memory.
   * Note: This is NOT thread-safe and should only be called when
   * no other threads are accessing the pool.
   */
  void Clear() {
    // Clear active pool
    BufferNode* current = active_pool_.Clear();
    while (current != nullptr) {
      BufferNode* next = static_cast<BufferNode*>(
          current->next.load(std::memory_order_relaxed));
      delete current;
      current = next;
    }
    pool_size_.store(0, std::memory_order_relaxed);

    // Clear retired list
    current = retired_pool_.Clear();
    while (current != nullptr) {
      BufferNode* next = static_cast<BufferNode*>(
          current->next.load(std::memory_order_relaxed));
      delete current;
      current = next;
    }
    retired_count_.store(0, std::memory_order_relaxed);
  }

  /**
   * @brief Gets the approximate number of buffers currently in the pool.
   *
   * Note: Due to the lock-free nature, this is an approximation and may
   * not reflect the exact state if concurrent operations are in progress.
   *
   * @return Approximate number of available buffers
   */
  size_t Size() const { return pool_size_.load(std::memory_order_relaxed); }

 private:
  /**
   * @brief Node in the lock-free Treiber stack.
   */
  struct BufferNode : public aifocore::TreiberNode<BufferNode> {
    std::vector<T> buffer;
  };

  /**
   * @brief Retires a node for deferred deletion.
   *
   * Instead of deleting immediately (which causes ABA problems), nodes are
   * moved to a retired list. When the retired list gets large enough, we
   * clean it up. This deferred reclamation approach prevents ABA issues.
   *
   * @param node Node to retire
   */
  void RetireNode(BufferNode* node) {
    // Push node onto the retired stack
    retired_pool_.Push(node);

    size_t count = retired_count_.fetch_add(1, std::memory_order_relaxed) + 1;

    // Periodically clean up retired nodes
    // We use a threshold to batch deletions for efficiency
    constexpr size_t kCleanupThreshold = 32;
    if (count >= kCleanupThreshold) {
      CleanupRetired();
    }
  }

  /**
   * @brief Cleans up retired nodes.
   *
   * This method is called periodically to actually delete nodes that have
   * been retired. By the time we get here, other threads should have
   * completed their operations, making it safe to delete.
   */
  void CleanupRetired() {
    // Atomically take ownership of the retired list
    BufferNode* to_delete = retired_pool_.Clear();
    if (to_delete == nullptr) {
      return;  // Another thread already cleaned up
    }

    // Reset count (approximate, but good enough)
    retired_count_.store(0, std::memory_order_relaxed);

    // Delete all nodes in the retired list
    while (to_delete != nullptr) {
      BufferNode* next = static_cast<BufferNode*>(
          to_delete->next.load(std::memory_order_relaxed));
      delete to_delete;
      to_delete = next;
    }
  }

  aifocore::TreiberStack<BufferNode> active_pool_;  ///< Active buffer pool
  std::atomic<size_t> pool_size_;                   ///< Approximate pool size
  aifocore::TreiberStack<BufferNode>
      retired_pool_;                   ///< Retired nodes for deferred deletion
  std::atomic<size_t> retired_count_;  ///< Approximate retired count
};

/**
 * @brief Gets a singleton instance of the byte buffer pool.
 *
 * This provides a global buffer pool for std::vector<uint8_t> that can be
 * shared across sources. This is the default buffer pool used for tile reading.
 *
 * @return Reference to the global byte buffer pool
 */
inline BufferPool<uint8_t>& GetBufferPool() {
  static BufferPool<uint8_t> pool;
  return pool;
}

/**
 * @brief Gets a singleton instance of the float buffer pool.
 *
 * This provides a global buffer pool for std::vector<float> that can be
 * shared across operators. Used for intermediate buffers in resize operations.
 *
 * @return Reference to the global float buffer pool
 */
inline BufferPool<float>& GetFloatBufferPool() {
  static BufferPool<float> pool;
  return pool;
}

}  // namespace fim

#endif  // AIFO_FIMAGE_INCLUDE_FIM_UTILITIES_BUFFER_POOL_H_

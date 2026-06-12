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
 * @file paste_test.cpp
 * @brief Tests for Paste operator class.
 * @author Jonas Teuwen
 * @date 2025
 */

#include "fim/operators/paste.h"

#include <gtest/gtest.h>

#include "fim/sources/black_source.h"
#include "fim/sources/memory_source.h"

namespace fim {
namespace {

// Helper function to create a solid-color image
MemorySource CreateSolidColorImage(int width, int height, int channels,
                                   uint8_t value) {
  std::vector<uint8_t> data(width * height * channels, value);
  ImageInfo dims(width, height, channels, PixelType::kUInt8,
                 DataLayout::kChannelsLast);
  return MemorySource::Create(std::move(data), dims);
}

TEST(PasteTest, BasicPaste) {
  // Create a black background (100x100, 3 channels)
  ImageInfo bg_dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);

  // Create a white foreground (20x20, 3 channels)
  auto foreground = CreateSolidColorImage(20, 20, 3, 255);

  // Paste at (10, 10)
  auto paste = Paste<BlackSource, MemorySource>(std::move(background),
                                                std::move(foreground), 10, 10);

  // Check dimensions
  auto dims = paste.GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 100);
  EXPECT_EQ(dims.GetHeight(), 100);
  EXPECT_EQ(dims.channels, 3);

  // Get tile that covers the pasted region
  auto tile = paste.GetTile(0, 0, 50, 50);

  // Check pixels in pasted region are white (255)
  for (int y = 10; y < 30; ++y) {
    for (int x = 10; x < 30; ++x) {
      int offset = (y * 50 + x) * 3;
      EXPECT_EQ(tile.GetData()[offset], 255) << "at (" << x << ", " << y << ")";
      EXPECT_EQ(tile.GetData()[offset + 1], 255)
          << "at (" << x << ", " << y << ")";
      EXPECT_EQ(tile.GetData()[offset + 2], 255)
          << "at (" << x << ", " << y << ")";
    }
  }

  // Check pixels outside pasted region are black (0)
  int offset = (0 * 50 + 0) * 3;  // Top-left corner
  EXPECT_EQ(tile.GetData()[offset], 0);
  EXPECT_EQ(tile.GetData()[offset + 1], 0);
  EXPECT_EQ(tile.GetData()[offset + 2], 0);
}

TEST(PasteTest, PasteAtOrigin) {
  // Create a black background
  ImageInfo bg_dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);

  // Create a gray foreground
  auto foreground = CreateSolidColorImage(30, 30, 3, 128);

  // Paste at (0, 0)
  auto paste = Paste<BlackSource, MemorySource>(std::move(background),
                                                std::move(foreground), 0, 0);

  // Get tile from top-left
  auto tile = paste.GetTile(0, 0, 40, 40);

  // Check pasted region is gray
  for (int y = 0; y < 30; ++y) {
    for (int x = 0; x < 30; ++x) {
      int offset = (y * 40 + x) * 3;
      EXPECT_EQ(tile.GetData()[offset], 128);
      EXPECT_EQ(tile.GetData()[offset + 1], 128);
      EXPECT_EQ(tile.GetData()[offset + 2], 128);
    }
  }

  // Check region outside paste is black
  int offset = (35 * 40 + 35) * 3;
  EXPECT_EQ(tile.GetData()[offset], 0);
}

TEST(PasteTest, PasteAtBottomRight) {
  // Create a black background
  ImageInfo bg_dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);

  // Create a red foreground
  std::vector<uint8_t> red_data;
  for (int i = 0; i < 10 * 10; ++i) {
    red_data.push_back(255);  // R
    red_data.push_back(0);    // G
    red_data.push_back(0);    // B
  }
  ImageInfo fg_dims(10, 10, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto foreground = MemorySource::Create(std::move(red_data), fg_dims);

  // Paste at (90, 90) - exactly at bottom-right corner
  auto paste = Paste<BlackSource, MemorySource>(std::move(background),
                                                std::move(foreground), 90, 90);

  // Get tile covering the pasted region
  auto tile = paste.GetTile(85, 85, 15, 15);

  // Check pasted region is red
  for (int y = 90; y < 100; ++y) {
    for (int x = 90; x < 100; ++x) {
      int tile_y = y - 85;
      int tile_x = x - 85;
      int offset = (tile_y * 15 + tile_x) * 3;
      EXPECT_EQ(tile.GetData()[offset], 255)
          << "R at (" << x << ", " << y << ")";
      EXPECT_EQ(tile.GetData()[offset + 1], 0)
          << "G at (" << x << ", " << y << ")";
      EXPECT_EQ(tile.GetData()[offset + 2], 0)
          << "B at (" << x << ", " << y << ")";
    }
  }
}

TEST(PasteTest, ChannelMismatch) {
  // Create background with 3 channels
  ImageInfo bg_dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);

  // Create foreground with 4 channels (RGBA)
  auto foreground = CreateSolidColorImage(20, 20, 4, 255);

  // Should throw because channels don't match
  EXPECT_THROW((Paste<BlackSource, MemorySource>(
                   std::move(background), std::move(foreground), 10, 10)),
               std::invalid_argument);
}

TEST(PasteTest, ForegroundExtendsBeforeHold) {
  // Create background
  ImageInfo bg_dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);

  // Create foreground
  auto foreground = CreateSolidColorImage(20, 20, 3, 255);

  // Try to paste at negative position
  EXPECT_THROW((Paste<BlackSource, MemorySource>(
                   std::move(background), std::move(foreground), -5, 10)),
               std::invalid_argument);
}

TEST(PasteTest, ForegroundExtendsBeyondBackground) {
  // Create background
  ImageInfo bg_dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);

  // Create foreground
  auto foreground = CreateSolidColorImage(20, 20, 3, 255);

  // Try to paste so it extends beyond background
  EXPECT_THROW((Paste<BlackSource, MemorySource>(
                   std::move(background), std::move(foreground), 95, 95)),
               std::invalid_argument);
}

TEST(PasteTest, MultiplePastesChained) {
  // Create black background
  ImageInfo bg_dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);

  // Create first foreground (white)
  auto fg1 = CreateSolidColorImage(20, 20, 3, 255);

  // First paste
  auto paste1 = Paste<BlackSource, MemorySource>(std::move(background),
                                                 std::move(fg1), 10, 10);

  // Create second foreground (gray)
  auto fg2 = CreateSolidColorImage(15, 15, 3, 128);

  // Second paste (overlapping with first)
  auto paste2 = Paste<Paste<BlackSource, MemorySource>, MemorySource>(
      std::move(paste1), std::move(fg2), 20, 20);

  // Get tile covering both pasted regions
  auto tile = paste2.GetTile(0, 0, 50, 50);

  // Check first paste region (not overlapped by second)
  int offset1 = (10 * 50 + 10) * 3;
  EXPECT_EQ(tile.GetData()[offset1], 255);

  // Check second paste region (should override first)
  int offset2 = (25 * 50 + 25) * 3;
  EXPECT_EQ(tile.GetData()[offset2], 128);

  // Check background region
  int offset3 = (5 * 50 + 5) * 3;
  EXPECT_EQ(tile.GetData()[offset3], 0);
}

TEST(PasteTest, TileNoOverlap) {
  // Create background and foreground
  ImageInfo bg_dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);
  auto foreground = CreateSolidColorImage(20, 20, 3, 255);

  // Paste at (50, 50)
  auto paste = Paste<BlackSource, MemorySource>(std::move(background),
                                                std::move(foreground), 50, 50);

  // Get tile that doesn't overlap with pasted region
  auto tile = paste.GetTile(0, 0, 30, 30);

  // All pixels should be black
  for (size_t i = 0; i < tile.GetData().size(); ++i) {
    EXPECT_EQ(tile.GetData()[i], 0);
  }
}

TEST(PasteTest, TilePartialOverlap) {
  // Create background and foreground
  ImageInfo bg_dims(100, 100, 3, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);
  auto foreground = CreateSolidColorImage(20, 20, 3, 255);

  // Paste at (40, 40)
  auto paste = Paste<BlackSource, MemorySource>(std::move(background),
                                                std::move(foreground), 40, 40);

  // Get tile that partially overlaps (covers 30-60, paste is at 40-60)
  auto tile = paste.GetTile(30, 30, 30, 30);

  // Check overlapping region is white
  for (int y = 40; y < 60; ++y) {
    for (int x = 40; x < 60; ++x) {
      int tile_y = y - 30;
      int tile_x = x - 30;
      int offset = (tile_y * 30 + tile_x) * 3;
      EXPECT_EQ(tile.GetData()[offset], 255);
    }
  }

  // Check non-overlapping region is black
  int offset = (0 * 30 + 0) * 3;
  EXPECT_EQ(tile.GetData()[offset], 0);
}

TEST(PasteTest, SingleChannelImage) {
  // Create grayscale background and foreground
  ImageInfo bg_dims(50, 50, 1, PixelType::kUInt8, DataLayout::kChannelsLast);
  auto background = BlackSource::Create(bg_dims);
  auto foreground = CreateSolidColorImage(10, 10, 1, 255);

  // Paste
  auto paste = Paste<BlackSource, MemorySource>(std::move(background),
                                                std::move(foreground), 20, 20);

  // Get tile
  auto tile = paste.GetTile(15, 15, 20, 20);

  // Check pasted region
  for (int y = 20; y < 30; ++y) {
    for (int x = 20; x < 30; ++x) {
      int tile_y = y - 15;
      int tile_x = x - 15;
      int offset = tile_y * 20 + tile_x;
      EXPECT_EQ(tile.GetData()[offset], 255);
    }
  }

  // Check background
  EXPECT_EQ(tile.GetData()[0], 0);
}

}  // namespace
}  // namespace fim

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
 * @file cpp_paste_example.cpp
 * @brief C++ example demonstrating black canvas and paste operations.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This example shows how to:
 * 1. Create a black canvas source
 * 2. Paste images at specific positions
 * 3. Chain multiple paste operations
 * 4. Combine paste with other pipeline operations (crop, resize)
 *
 * Build with:
 *   bazelisk build //aifo/fimage/examples:cpp_paste_example
 *
 * Run with:
 *   bazel-bin/aifo/fimage/examples/cpp_paste_example
 */

#include <iostream>
#include <vector>

#include "fim/image.h"
#include "fim/operators/paste.h"
#include "fim/sources/black_source.h"
#include "fim/sources/memory_source.h"
#include "fim/types.h"

namespace fim_example {

/**
 * @brief Creates a solid-color image in memory.
 *
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels
 * @param value Pixel value (0-255) for all channels
 * @return MemorySource containing the solid-color image
 */
fim::MemorySource CreateSolidColorImage(int width, int height, int channels,
                                        uint8_t value) {
  std::vector<uint8_t> data(width * height * channels, value);
  fim::ImageInfo dims(width, height, channels, fim::PixelType::kUInt8,
                      fim::DataLayout::kChannelsLast);
  return fim::MemorySource::Create(std::move(data), dims);
}

/**
 * @brief Creates an RGB image with specified color.
 *
 * @param width Image width
 * @param height Image height
 * @param r Red channel value (0-255)
 * @param g Green channel value (0-255)
 * @param b Blue channel value (0-255)
 * @return MemorySource containing the colored image
 */
fim::MemorySource CreateColoredImage(int width, int height, uint8_t r,
                                     uint8_t g, uint8_t b) {
  std::vector<uint8_t> data;
  data.reserve(width * height * 3);

  for (int i = 0; i < width * height; ++i) {
    data.push_back(r);
    data.push_back(g);
    data.push_back(b);
  }

  fim::ImageInfo dims(width, height, 3, fim::PixelType::kUInt8,
                      fim::DataLayout::kChannelsLast);
  return fim::MemorySource::Create(std::move(data), dims);
}

/**
 * @brief Example 1: Basic paste operation.
 *
 * Demonstrates creating a black canvas and pasting a single colored square.
 */
void ExampleBasicPaste() {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "EXAMPLE 1: Basic Paste Operation\n";
  std::cout << std::string(60, '=') << "\n\n";

  // Create a black canvas (500x500, 3 channels)
  fim::ImageInfo canvas_dims(500, 500, 3, fim::PixelType::kUInt8,
                             fim::DataLayout::kChannelsLast);
  auto black_canvas = fim::BlackSource::Create(canvas_dims);

  std::cout << "✓ Created black canvas: " << canvas_dims.GetWidth() << "x"
            << canvas_dims.GetHeight() << ", " << canvas_dims.channels
            << " channels\n";

  // Create a red square (100x100)
  auto red_square = CreateColoredImage(100, 100, 255, 0, 0);
  std::cout << "✓ Created red square: 100x100\n";

  // Paste the red square at position (200, 200)
  auto result = fim::Paste<fim::BlackSource, fim::MemorySource>(
      std::move(black_canvas), std::move(red_square), 200, 200);

  std::cout << "✓ Pasted red square at (200, 200)\n";

  // Render to PNG file
  std::cout << "\nSaving to: example_basic_paste.png\n";
  result.Render(fim::LodePngSink::Create("example_basic_paste.png"));

  std::cout << "✓ Done!\n";
}

/**
 * @brief Example 2: Multiple chained paste operations.
 *
 * Demonstrates pasting multiple colored squares onto a black canvas.
 */
void ExampleMultiplePastes() {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "EXAMPLE 2: Multiple Chained Pastes\n";
  std::cout << std::string(60, '=') << "\n\n";

  // Create black canvas
  fim::ImageInfo canvas_dims(800, 800, 3, fim::PixelType::kUInt8,
                             fim::DataLayout::kChannelsLast);
  auto canvas = fim::BlackSource::Create(canvas_dims);

  // Create colored squares
  auto red = CreateColoredImage(150, 150, 255, 0, 0);
  auto green = CreateColoredImage(150, 150, 0, 255, 0);
  auto blue = CreateColoredImage(150, 150, 0, 0, 255);

  std::cout << "✓ Created 3 colored squares (150x150)\n";

  // First paste: red square
  auto paste1 = fim::Paste<fim::BlackSource, fim::MemorySource>(
      std::move(canvas), std::move(red), 100, 100);

  std::cout << "✓ Pasted red at (100, 100)\n";

  // Second paste: green square (on top of result)
  auto paste2 = fim::Paste<fim::Paste<fim::BlackSource, fim::MemorySource>,
                           fim::MemorySource>(std::move(paste1),
                                              std::move(green), 300, 300);

  std::cout << "✓ Pasted green at (300, 300)\n";

  // Third paste: blue square
  auto paste3 =
      fim::Paste<fim::Paste<fim::Paste<fim::BlackSource, fim::MemorySource>,
                            fim::MemorySource>,
                 fim::MemorySource>(std::move(paste2), std::move(blue), 500,
                                    500);

  std::cout << "✓ Pasted blue at (500, 500)\n";

  // Render
  std::cout << "\nSaving to: example_multiple_pastes.png\n";
  paste3.Render(fim::LodePngSink::Create("example_multiple_pastes.png"));

  std::cout << "✓ Done!\n";
}

/**
 * @brief Example 3: Paste combined with pipeline operations.
 *
 * Demonstrates using paste with crop, downsample, and other operations.
 */
void ExamplePasteWithPipeline() {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "EXAMPLE 3: Paste + Pipeline Operations\n";
  std::cout << std::string(60, '=') << "\n\n";

  // Create black canvas
  fim::ImageInfo canvas_dims(2048, 2048, 3, fim::PixelType::kUInt8,
                             fim::DataLayout::kChannelsLast);
  auto canvas = fim::BlackSource::Create(canvas_dims);

  // Create colored squares
  auto cyan = CreateColoredImage(300, 300, 0, 255, 255);
  auto magenta = CreateColoredImage(300, 300, 255, 0, 255);

  std::cout << "✓ Created black canvas: 2048x2048\n";
  std::cout << "✓ Created colored squares: 300x300\n";

  // Paste two squares
  auto paste1 = fim::Paste<fim::BlackSource, fim::MemorySource>(
      std::move(canvas), std::move(cyan), 400, 400);

  auto paste2 = fim::Paste<fim::Paste<fim::BlackSource, fim::MemorySource>,
                           fim::MemorySource>(std::move(paste1),
                                              std::move(magenta), 1000, 1000);

  std::cout << "✓ Pasted cyan at (400, 400) and magenta at (1000, 1000)\n";

  // Wrap in Image for fluent API
  auto img =
      fim::Image<fim::Paste<fim::Paste<fim::BlackSource, fim::MemorySource>,
                            fim::MemorySource>>(std::move(paste2));

  // Chain operations: crop, then downsample, then resize
  std::cout << "\nApplying pipeline operations:\n";
  std::cout << "  1. Crop to (300, 300, 1500, 1500)\n";
  std::cout << "  2. Downsample by 2\n";
  std::cout << "  3. Resize to 512x512 with Magic2021 kernel\n";

  std::move(img)
      .Crop(300, 300, 1500, 1500)
      .Downsample(2)
      .Resize(512, 512, fim::resize::KernelType::kMagic2021)
      .Render(fim::LodePngSink::Create("example_paste_pipeline.png"));

  std::cout << "\nSaving to: example_paste_pipeline.png\n";
  std::cout << "✓ Done!\n";
}

/**
 * @brief Example 4: Using paste for tile-based assembly.
 *
 * Demonstrates creating a checkerboard pattern using paste operations.
 */
void ExampleTileAssembly() {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "EXAMPLE 4: Tile Assembly (Checkerboard)\n";
  std::cout << std::string(60, '=') << "\n\n";

  const int tile_size = 64;
  const int grid_size = 8;  // 8x8 grid
  const int canvas_size = tile_size * grid_size;

  // Create black canvas
  fim::ImageInfo canvas_dims(canvas_size, canvas_size, 3,
                             fim::PixelType::kUInt8,
                             fim::DataLayout::kChannelsLast);
  auto canvas = fim::BlackSource::Create(canvas_dims);

  // Create white tile (we'll use black from canvas for black tiles)
  auto white_tile = CreateSolidColorImage(tile_size, tile_size, 3, 255);

  std::cout << "✓ Creating " << grid_size << "x" << grid_size
            << " checkerboard\n";
  std::cout << "  Canvas size: " << canvas_size << "x" << canvas_size << "\n";
  std::cout << "  Tile size: " << tile_size << "x" << tile_size << "\n\n";

  // This example demonstrates the concept, but in practice you'd need to
  // handle the type complexity. For simplicity, let's paste a few tiles.

  // First white tile at (0, 0)
  auto paste1 = fim::Paste<fim::BlackSource, fim::MemorySource>(
      std::move(canvas), CreateSolidColorImage(tile_size, tile_size, 3, 255), 0,
      0);

  // Second white tile at (128, 0)
  auto paste2 = fim::Paste<fim::Paste<fim::BlackSource, fim::MemorySource>,
                           fim::MemorySource>(
      std::move(paste1), CreateSolidColorImage(tile_size, tile_size, 3, 255),
      128, 0);

  // Third white tile at (0, 128)
  auto paste3 =
      fim::Paste<fim::Paste<fim::Paste<fim::BlackSource, fim::MemorySource>,
                            fim::MemorySource>,
                 fim::MemorySource>(
          std::move(paste2),
          CreateSolidColorImage(tile_size, tile_size, 3, 255), 0, 128);

  std::cout << "✓ Pasted white tiles in checkerboard pattern\n";
  std::cout << "\nSaving to: example_tile_assembly.png\n";
  paste3.Render(fim::LodePngSink::Create("example_tile_assembly.png"));

  std::cout << "✓ Done!\n";
  std::cout << "\nNote: For full checkerboard, use Python API or create a\n";
  std::cout << "helper function that handles the type complexity.\n";
}

}  // namespace fim_example

int main() {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "FIM C++ PASTE EXAMPLES\n";
  std::cout << std::string(60, '=') << "\n";
  std::cout << "\nThese examples demonstrate the black canvas and paste\n";
  std::cout << "functionality in the C++ API:\n";
  std::cout << "- BlackSource for lazy black canvas generation\n";
  std::cout << "- Paste operator for compositing images\n";
  std::cout << "- Chaining multiple paste operations\n";
  std::cout << "- Combining paste with other pipeline operations\n";

  try {
    // Run all examples
    fim_example::ExampleBasicPaste();
    fim_example::ExampleMultiplePastes();
    fim_example::ExamplePasteWithPipeline();
    fim_example::ExampleTileAssembly();

    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "ALL EXAMPLES COMPLETE!\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << "\nGenerated files:\n";
    std::cout << "  - example_basic_paste.png\n";
    std::cout << "  - example_multiple_pastes.png\n";
    std::cout << "  - example_paste_pipeline.png\n";
    std::cout << "  - example_tile_assembly.png\n";

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}

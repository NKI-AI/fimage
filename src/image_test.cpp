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

#include "fim/image.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "fim/types.h"
#include "lodepng/lodepng.h"

namespace fim {

class ImageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test PNG file
    CreateTestPngFile("test_image.png", 100, 80, 3);
  }

  void TearDown() override {
    // Clean up test files
    std::filesystem::remove("test_image.png");
    std::filesystem::remove("test_output.png");
    std::filesystem::remove("test_output.tiff");
  }

 private:
  void CreateTestPngFile(const std::string& filename, int width, int height,
                         int channels) {
    std::vector<uint8_t> image_data(width * height * channels);

    // Fill with colorful pattern (prevents auto-optimization to grayscale)
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int pixel_offset = (y * width + x) * channels;
        bool is_black = ((x / 8) + (y / 8)) % 2 == 0;

        if (channels == 3) {
          // Create colorful checkerboard with different RGB values
          image_data[pixel_offset + 0] = is_black ? 255 : (x % 256);        // R
          image_data[pixel_offset + 1] = is_black ? (y % 256) : 255;        // G
          image_data[pixel_offset + 2] = is_black ? ((x + y) % 256) : 128;  // B
        } else {
          // Fallback for other channel counts (grayscale, RGBA, etc.)
          uint8_t value = is_black ? 0 : 255;
          for (int c = 0; c < channels; ++c) {
            image_data[pixel_offset + c] = value;
          }
        }
      }
    }

    // Write PNG file using lodepng
    unsigned error = lodepng_encode24_file(filename.c_str(), image_data.data(),
                                           width, height);
    if (error != 0) {
      throw std::runtime_error("Failed to create test PNG file: " +
                               std::string(lodepng_error_text(error)));
    }
  }
};

// Test CreatePngImage function
TEST_F(ImageTest, CreatePngImage) {
  auto image = fim::CreatePngImage("test_image.png");

  auto dims = image.GetSource().GetDimensions();
  EXPECT_EQ(dims.GetWidth(), 100);
  EXPECT_EQ(dims.GetHeight(), 80);
  EXPECT_EQ(dims.channels, 3);  // PNG source preserves RGB format
}

// Test CreateTiffImage function (if file exists)
TEST_F(ImageTest, CreateTiffImage) {
  if (!std::filesystem::exists("checkerboard.svs")) {
    GTEST_SKIP() << "Test TIFF file not found: checkerboard.svs";
  }

  auto image = fim::CreateTiffImage("checkerboard.svs");

  auto dims = image.GetSource().GetDimensions();
  EXPECT_GT(dims.GetWidth(), 0);
  EXPECT_GT(dims.GetHeight(), 0);
  EXPECT_GT(dims.channels, 0);
}

// Test Image crop operation
TEST_F(ImageTest, CropOperation) {
  auto image = fim::CreatePngImage("test_image.png");

  auto cropped = std::move(image).Crop(10, 10, 50, 40);

  // The crop should return a new image with crop operator
  // We can't directly test the type, but we can test functionality
  std::move(cropped).Render(fim::LodePngSink::Create("test_output.png"));

  EXPECT_TRUE(std::filesystem::exists("test_output.png"));
  EXPECT_GT(std::filesystem::file_size("test_output.png"), 0);
}

// Test Image downsample operation
TEST_F(ImageTest, DownsampleOperation) {
  auto image = fim::CreatePngImage("test_image.png");

  auto downsampled = std::move(image).Downsample(2);

  // The downsample should return a new image with downsample operator
  std::move(downsampled).Render(fim::LodePngSink::Create("test_output.png"));

  EXPECT_TRUE(std::filesystem::exists("test_output.png"));
  EXPECT_GT(std::filesystem::file_size("test_output.png"), 0);
}

// Test chained operations
TEST_F(ImageTest, ChainedOperations) {
  auto image = fim::CreatePngImage("test_image.png");

  // Chain crop and downsample operations
  std::move(image)
      .Crop(5, 5, 80, 60)
      .Downsample(2)
      .Render(fim::LodePngSink::Create("test_output.png"));

  EXPECT_TRUE(std::filesystem::exists("test_output.png"));
  EXPECT_GT(std::filesystem::file_size("test_output.png"), 0);
}

// Test render to PNG sink
TEST_F(ImageTest, RenderToPngSink) {
  auto image = fim::CreatePngImage("test_image.png");

  std::move(image).Render(fim::LodePngSink::Create("test_output.png"));

  EXPECT_TRUE(std::filesystem::exists("test_output.png"));
  EXPECT_GT(std::filesystem::file_size("test_output.png"), 0);
}

// Test render to TIFF sink
TEST_F(ImageTest, RenderToTiffSink) {
  auto image = fim::CreatePngImage("test_image.png");

  std::move(image).Render(fim::TiffSink::Create("test_output.tiff"));

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test render to TIFF sink with custom tile size
TEST_F(ImageTest, RenderToTiffSinkWithTileSize) {
  auto image = fim::CreatePngImage("test_image.png");

  std::move(image).Render(
      fim::TiffSink::Create("test_output.tiff", fim::TileSize(32, 32)));

  EXPECT_TRUE(std::filesystem::exists("test_output.tiff"));
  EXPECT_GT(std::filesystem::file_size("test_output.tiff"), 0);
}

// Test GetSource method
TEST_F(ImageTest, GetSource) {
  auto image = fim::CreatePngImage("test_image.png");

  const auto& source = image.GetSource();
  auto dims = source.GetDimensions();

  EXPECT_EQ(dims.GetWidth(), 100);
  EXPECT_EQ(dims.GetHeight(), 80);
  EXPECT_EQ(dims.channels, 3);  // PNG source preserves RGB format
}

// Test error handling
TEST_F(ImageTest, ErrorHandling) {
  // Test with non-existent PNG file
  EXPECT_THROW(fim::CreatePngImage("non_existent.png"), std::runtime_error);

  // Test with non-existent TIFF file
  EXPECT_THROW(fim::CreateTiffImage("non_existent.tiff"), std::runtime_error);

  // Test invalid downsample factor
  auto image1 = fim::CreatePngImage("test_image.png");
  EXPECT_THROW(std::move(image1).Downsample(0), std::invalid_argument);

  auto image2 = fim::CreatePngImage("test_image.png");
  EXPECT_THROW(std::move(image2).Downsample(-1), std::invalid_argument);
}

// Test complex pipeline
TEST_F(ImageTest, ComplexPipeline) {
  auto image = fim::CreatePngImage("test_image.png");

  // Multiple operations in sequence
  std::move(image)
      .Crop(10, 10, 80, 60)
      .Downsample(2)
      .Crop(5, 5, 30, 25)
      .Render(fim::LodePngSink::Create("test_output.png"));

  EXPECT_TRUE(std::filesystem::exists("test_output.png"));
  EXPECT_GT(std::filesystem::file_size("test_output.png"), 0);
}

}  // namespace fim

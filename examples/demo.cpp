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
#include <fim/sinks/tiff_sink.h>
#include <fim/sources/tiff_source.h>

#include <iostream>
#include <string>

#include "CLI11/CLI11.hpp"

int main(int argc, char* argv[]) {
  std::string input_file;
  bool pyramidal = false;

  CLI::App app{
      "FImage Demo - Process images with crop, downsample, and TIFF output"};

  app.add_option("-f,--file", input_file, "Path to the input image file")
      ->required();

  app.add_flag("--pyramidal", pyramidal,
               "Create a pyramidal TIFF with multiple levels");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return app.exit(e);
  }

  try {
    // Get image info first
    auto info_image = fim::CreateTiffImage(input_file);
    auto dims = info_image.GetSource().GetDimensions();
    auto tile_size = info_image.GetSource().GetIdealTileSize();

    std::cout << "Input file: " << input_file << std::endl;
    std::cout << "Input image dimensions: " << dims.GetWidth() << "x"
              << dims.GetHeight() << " channels: " << dims.channels
              << std::endl;
    std::cout << "Ideal tile size: " << tile_size.width << "x"
              << tile_size.height << std::endl;

    if (pyramidal) {
      std::cout << "\n=== Creating PYRAMIDAL TIFF ===\n";
      std::cout << "This will create a multi-page TIFF with progressively "
                   "smaller levels\n";
      std::cout << "until dimensions are smaller than the tile size.\n\n";

      // Create a pyramidal TIFF that should continue to smaller levels
      fim::CreateTiffImage(input_file)
          .Crop(1600, 1600, 3200, 6400)
          .Downsample(2)
          .Render(fim::TiffSink::Create("crop_test_pyramidal.tiff",
                                        fim::TileSize(512, 512),
                                        true,  // pyramidal = true
                                        10,    // 10MB memory threshold
                                        2));   // downsample factor

      std::cout << "Pyramidal TIFF created: crop_test_pyramidal.tiff\n";
      std::cout << "Use 'tiffinfo crop_test_pyramidal.tiff' to inspect the "
                   "pyramid levels.\n";

    } else {
      std::cout << "\n=== Creating SINGLE-PAGE TIFF ===\n";
      std::cout << "This will create a standard single-page TIFF file.\n\n";

      // Create a single-page TIFF (traditional behavior)
      fim::CreateTiffImage(input_file)
          .Crop(1600, 1600, 3200, 6400)
          .Downsample(2)
          .Render(fim::TiffSink::Create(
              "crop_test_single.tiff",
              fim::TileSize(512, 512)));  // pyramidal = false (default)

      std::cout << "Single-page TIFF created: crop_test_single.tiff\n";
      std::cout
          << "Use 'tiffinfo crop_test_single.tiff' to inspect the file.\n";
    }

    std::cout << "\n=== Demo completed successfully! ===\n";
    std::cout << "Try running with different options:\n";
    std::cout << "  " << argv[0]
              << " --file input.tiff          # Process a different file\n";
    std::cout << "  " << argv[0]
              << " --pyramidal               # Creates pyramidal TIFF\n";
    std::cout << "  " << argv[0]
              << " -f input.svs --pyramidal  # Custom file with pyramid\n";

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

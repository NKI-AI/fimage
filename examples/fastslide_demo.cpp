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
#include <fim/sinks/lodepng_png_sink.h>
#include <fim/sinks/tiff_sink.h>
#include <fim/sources/fastslide_source.h>

#include <iomanip>
#include <iostream>

#include "CLI11/CLI11.hpp"

int main(int argc, char* argv[]) {
  std::string input_file;
  int level = 0;
  int tile_x = 0;
  int tile_y = 0;
  int tile_width = 512;
  int tile_height = 512;
  bool show_metadata = false;

  CLI::App app{
      "FImage FastSlide Demo - Process whole slide images with "
      "multi-level pyramid support"};

  app.add_option("-f,--file", input_file, "Path to the whole slide image file")
      ->required();

  app.add_option("-l,--level", level, "Pyramid level to use (default: 0)")
      ->default_val(0);

  app.add_flag("-m,--metadata", show_metadata,
               "Show detailed metadata for all levels");

  // Positional arguments for tile coordinates and size
  app.add_option("x", tile_x, "X coordinate for tile extraction")
      ->default_val(0);

  app.add_option("y", tile_y, "Y coordinate for tile extraction")
      ->default_val(0);

  app.add_option("width", tile_width, "Tile width for extraction")
      ->default_val(512);

  app.add_option("height", tile_height, "Tile height for extraction")
      ->default_val(512);

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return app.exit(e);
  }

  try {
    std::cout << "=== FImage FastSlide Demo ===\n\n";
    std::cout << "Opening slide: " << input_file << "\n\n";

    // Open the slide
    auto source = fim::FastSlideSource::Create(input_file);

    // Display basic metadata
    std::cout << "Format: " << source.GetFormatName() << "\n";
    std::cout << "Pyramid levels: " << source.GetLevelCount() << "\n";

    auto mpp = source.GetMpp();
    std::cout << "Microns per pixel (MPP): " << std::fixed
              << std::setprecision(4) << mpp[0] << " x " << mpp[1] << "\n\n";

    // Display level information
    if (show_metadata) {
      std::cout << "=== Pyramid Level Details ===\n\n";
      int channels = source.GetNumChannels();
      for (int i = 0; i < source.GetLevelCount(); ++i) {
        auto dims = source.GetLevelDimensions(i);
        double downsample = source.GetLevelDownsample(i);

        std::cout << "Level " << i << ":\n";
        std::cout << "  Dimensions: " << dims.width << " x " << dims.height
                  << " x " << channels << " channels\n";
        std::cout << "  Downsample: " << std::fixed << std::setprecision(2)
                  << downsample << "x\n";
        std::cout << "  MPP at level: " << std::fixed << std::setprecision(4)
                  << (mpp[0] * downsample) << " x " << (mpp[1] * downsample)
                  << "\n\n";
      }
    }

    // Validate requested level
    if (level < 0 || level >= source.GetLevelCount()) {
      std::cerr << "Error: Invalid level " << level << ". Valid range: 0-"
                << (source.GetLevelCount() - 1) << "\n";
      return 1;
    }

    // Get level view
    auto level_view = source.LevelView(level);
    auto dims = level_view.GetDimensions();
    auto tile_size = level_view.GetIdealTileSize();

    std::cout << "=== Working with Level " << level << " ===\n\n";
    std::cout << "Level dimensions: " << dims.GetWidth() << " x "
              << dims.GetHeight() << " x " << dims.channels << " channels\n";
    std::cout << "Native tile size: " << tile_size.width << " x "
              << tile_size.height << "\n";
    std::cout << "Downsample factor: " << std::fixed << std::setprecision(2)
              << source.GetLevelDownsample(level) << "x\n\n";

    // Example 1: Extract a single tile
    std::cout << "=== Example 1: Extract Single Tile ===\n\n";
    std::cout << "Extracting tile at (" << tile_x << ", " << tile_y
              << ") with size " << tile_width << "x" << tile_height << "\n";

    auto tile = level_view.GetTile(tile_x, tile_y, tile_width, tile_height);
    std::cout << "Tile extracted: " << tile.width << "x" << tile.height << " x "
              << tile.channels << " channels\n";
    std::cout << "Data size: " << tile.GetData().size() << " bytes\n\n";

    // Example 2: Crop and downsample using fimage pipeline
    std::cout << "=== Example 2: Crop and Downsample Pipeline ===\n\n";

    int crop_x = std::min(tile_x, dims.GetWidth() - 1024);
    int crop_y = std::min(tile_y, dims.GetHeight() - 1024);
    int crop_size = std::min(
        1024, std::min(dims.GetWidth() - crop_x, dims.GetHeight() - crop_y));

    std::cout << "Creating pipeline:\n";
    std::cout << "  1. Level " << level << " view\n";
    std::cout << "  2. Crop at (" << crop_x << ", " << crop_y << ") size "
              << crop_size << "x" << crop_size << "\n";
    std::cout << "  3. Downsample by 2x\n";
    std::cout << "  4. Save as PNG\n\n";

    fim::Image(source.LevelView(level))
        .Crop(crop_x, crop_y, crop_size, crop_size)
        .Downsample(2)
        .Render(fim::LodePngSink::Create("fastslide_output.png"));

    std::cout << "Output saved to: fastslide_output.png\n";
    std::cout << "Output dimensions: " << (crop_size / 2) << "x"
              << (crop_size / 2) << "\n\n";

    // Example 3: Multi-level extraction
    if (source.GetLevelCount() > 1) {
      std::cout << "=== Example 3: Multi-Level Extraction ===\n\n";
      std::cout
          << "Extracting tiles from different levels at same coordinates:\n";
      std::cout
          << "Note: Coordinates are in each level's coordinate system\n\n";

      for (int lvl = 0; lvl < std::min(3, source.GetLevelCount()); ++lvl) {
        auto lvl_view = source.LevelView(lvl);
        auto lvl_dims = lvl_view.GetDimensions();

        // Extract a small tile from each level
        int extract_x = std::min(100, lvl_dims.GetWidth() - 256);
        int extract_y = std::min(100, lvl_dims.GetHeight() - 256);
        int extract_size =
            std::min(256, std::min(lvl_dims.GetWidth() - extract_x,
                                   lvl_dims.GetHeight() - extract_y));

        auto lvl_tile =
            lvl_view.GetTile(extract_x, extract_y, extract_size, extract_size);

        std::string output_name =
            "fastslide_level" + std::to_string(lvl) + ".png";
        fim::Image(source.LevelView(lvl))
            .Crop(extract_x, extract_y, extract_size, extract_size)
            .Render(fim::LodePngSink::Create(output_name));

        std::cout << "Level " << lvl << ": " << lvl_tile.width << "x"
                  << lvl_tile.height << " -> " << output_name << "\n";
      }
      std::cout << "\n";
    }

    // Example 4: Create a pyramidal TIFF from a region
    std::cout << "=== Example 4: Create Pyramidal TIFF ===\n\n";

    int region_size =
        std::min(2048, std::min(dims.GetWidth(), dims.GetHeight()) / 2);
    int region_x = dims.GetWidth() / 4;
    int region_y = dims.GetHeight() / 4;

    std::cout << "Extracting region from level " << level << ":\n";
    std::cout << "  Position: (" << region_x << ", " << region_y << ")\n";
    std::cout << "  Size: " << region_size << "x" << region_size << "\n";
    std::cout << "  Creating pyramidal TIFF...\n\n";

    fim::Image(source.LevelView(level))
        .Crop(region_x, region_y, region_size, region_size)
        .Render(fim::TiffSink::Create("fastslide_pyramid.tiff",
                                      fim::TileSize(512, 512),
                                      true,  // pyramidal = true
                                      1000,  // 1000MB memory threshold
                                      2));   // downsample factor

    std::cout << "Pyramidal TIFF created: fastslide_pyramid.tiff\n";
    std::cout << "Use 'tiffinfo fastslide_pyramid.tiff' to inspect pyramid "
                 "levels.\n\n";

    std::cout << "=== Demo completed successfully! ===\n\n";
    std::cout << "Try different options:\n";
    std::cout << "  " << argv[0]
              << " -f slide.svs --metadata              # Show all metadata\n";
    std::cout
        << "  " << argv[0]
        << " -f slide.svs -l 2                    # Use pyramid level 2\n";
    std::cout << "  " << argv[0]
              << " -f slide.svs 1000 2000 512 512       # Extract specific "
                 "tile at x,y with size\n";
    std::cout << "  " << argv[0]
              << " -f slide.svs -l 1 2000 3000 1024 1024  # Level 1 tile at "
                 "custom position and size\n";

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

Examples
========

This section provides complete working examples of using fimage for various tasks.

Overview
--------

The examples demonstrate:

- **Basic usage**: Simple image processing pipelines
- **Advanced techniques**: Complex transformations and optimizations
- **Integration**: Using fimage with deep learning frameworks
- **Performance**: Efficient processing of large images

C++ Examples
------------

demo.cpp
~~~~~~~~

**Location**: `examples/demo.cpp`

Basic demonstration of the fimage C++ API with a simple processing pipeline.

**What it demonstrates:**

- Loading a TIFF image
- Applying crop operation
- Applying downsample operation
- Saving to PNG format

**Code walkthrough:**

.. code-block:: cpp

   #include <fim/image.h>
   #include <fim/sinks/lodepng_png_sink.h>
   #include <fim/sources/tiff_source.h>
   
   int main() {
       // Create image from TIFF file
       auto image = fim::CreateTiffImage("input.tiff");
       
       // Build processing pipeline (lazy evaluation)
       auto result = std::move(image)
           .Crop(100, 100, 512, 512)    // Extract 512x512 region
           .Downsample(2);               // Reduce to 256x256
       
       // Materialize result (evaluation happens here)
       result.Render(fim::LodePngSink("output.png"));
       
       return 0;
   }

**Key concepts:**

- **Lazy evaluation**: Operations build a graph, execution deferred until Render()
- **Move semantics**: Each operation consumes the previous stage
- **Type safety**: Full pipeline type known at compile time
- **Zero overhead**: CRTP enables complete inlining

**Building:**

.. code-block:: bash

   bazelisk build //aifo/fimage/examples:demo
   bazel-bin/aifo/fimage/examples/demo

**Expected output:**

- Creates `output.png` with 256x256 cropped and downsampled region
- No console output unless errors occur

checkerboard_demo.cpp
~~~~~~~~~~~~~~~~~~~~~

**Location**: `examples/checkerboard_demo.cpp`

More complex example demonstrating chained operations and error handling.

**What it demonstrates:**

- Creating images from memory
- Multiple operator chaining
- Error handling with try-catch
- Different sink formats

**Code walkthrough:**

.. code-block:: cpp

   #include <fim/image.h>
   #include <fim/sources/memory_source.h>
   #include <fim/sinks/tiff_sink.h>
   #include <iostream>
   #include <vector>
   
   // Generate checkerboard pattern
   std::vector<uint8_t> GenerateCheckerboard(int width, int height, int square_size) {
       std::vector<uint8_t> data(width * height * 3);
       
       for (int y = 0; y < height; ++y) {
           for (int x = 0; x < width; ++x) {
               bool black = ((x / square_size) + (y / square_size)) % 2 == 0;
               uint8_t color = black ? 0 : 255;
               
               int offset = (y * width + x) * 3;
               data[offset + 0] = color;  // R
               data[offset + 1] = color;  // G
               data[offset + 2] = color;  // B
           }
       }
       
       return data;
   }
   
   int main() {
       try {
           // Generate 1024x1024 checkerboard
           auto data = GenerateCheckerboard(1024, 1024, 64);
           
           // Create memory source
           fim::MemorySource source(data, 1024, 1024, 3);
           
           // Process with multiple operations
           fim::Image(std::move(source))
               .Crop(256, 256, 512, 512)  // Extract center
               .Downsample(2)              // 256x256
               .Render(fim::TiffSink("checkerboard_result.tiff"));
           
           std::cout << "Successfully created checkerboard_result.tiff" << std::endl;
           
       } catch (const std::exception& e) {
           std::cerr << "Error: " << e.what() << std::endl;
           return 1;
       }
       
       return 0;
   }

**Key concepts:**

- **Memory sources**: Creating images without file I/O
- **Procedural generation**: Building test data programmatically
- **Error handling**: Catching and reporting exceptions
- **Complex chains**: Multiple operations in sequence

**Building:**

.. code-block:: bash

   bazelisk build //aifo/fimage/examples:checkerboard_demo
   bazel-bin/aifo/fimage/examples/checkerboard_demo

**Expected output:**

- Creates `checkerboard_result.tiff` 
- Prints success message or error details

Python Examples
---------------

example.py
~~~~~~~~~~

**Location**: `python/example.py`

Comprehensive Python API demonstration.

**What it demonstrates:**

- Loading images from various sources
- Pipeline operations with lazy evaluation
- NumPy integration
- Stacking operations
- Error handling

**Code walkthrough:**

.. code-block:: python

   import fim
   import numpy as np
   
   def basic_pipeline():
       """Basic image processing pipeline."""
       # Load TIFF image (lazy)
       img = fim.Image.from_libtiff("input.tiff")
       
       # Apply operations (lazy - no computation yet)
       result = img.crop(100, 100, 512, 512).downsample(2)
       
       # Materialize result
       result.write_png("output.png")
       print("Created output.png")
   
   def numpy_integration():
       """Demonstrate NumPy integration."""
       # Load image
       img = fim.Image.from_libtiff("input.tiff")
       
       # Convert to NumPy (zero-copy when possible)
       array = img.to_numpy()
       print(f"Array shape: {array.shape}, dtype: {array.dtype}")
       
       # Process with NumPy
       normalized = array.astype(np.float32) / 255.0
       
       # Create new image from processed array
       processed = fim.Image.from_memory(
           normalized.flatten().tolist(),
           array.shape[1],  # width
           array.shape[0],  # height
           array.shape[2]   # channels
       )
       
       processed.write_tiff("normalized.tiff")
   
   def stacking_example():
       """Demonstrate image stacking."""
       # Load individual channels
       red = fim.Image.from_png("red_channel.png")
       green = fim.Image.from_png("green_channel.png")
       blue = fim.Image.from_png("blue_channel.png")
       
       # Stack into RGB
       rgb = fim.Image.stack([red, green, blue], axis="bands")
       
       # Process composite
       rgb.crop(0, 0, 1024, 1024).write_tiff("rgb_composite.tiff")
   
   if __name__ == "__main__":
       try:
           basic_pipeline()
           numpy_integration()
           stacking_example()
       except Exception as e:
           print(f"Error: {e}")

**Key concepts:**

- **Lazy evaluation**: Same as C++, deferred until output
- **NumPy interop**: Seamless conversion to/from NumPy arrays
- **Stacking**: Combining multiple images
- **Error handling**: Python exception handling

**Running:**

.. code-block:: bash

   bazelisk run //aifo/fimage/python:example

**Expected output:**

- Creates several output files
- Prints progress messages

Integration Examples
--------------------

PyTorch Dataset
~~~~~~~~~~~~~~~

Using fimage as a PyTorch Dataset for efficient tile extraction:

.. code-block:: python

   import fim
   import torch
   from torch.utils.data import Dataset, DataLoader
   
   class FimageDataset(Dataset):
       """PyTorch Dataset using fimage for efficient tile loading."""
       
       def __init__(self, image_path, tile_size=224, stride=112):
           self.image = fim.Image.from_libtiff(image_path)
           self.tile_size = tile_size
           self.stride = stride
           
           # Get dimensions
           width, height, channels = self.image.dimensions
           
           # Generate tile positions
           self.positions = []
           for y in range(0, height - tile_size + 1, stride):
               for x in range(0, width - tile_size + 1, stride):
                   self.positions.append((x, y))
       
       def __len__(self):
           return len(self.positions)
       
       def __getitem__(self, idx):
           x, y = self.positions[idx]
           
           # Lazy tile extraction
           tile = self.image.crop(x, y, self.tile_size, self.tile_size)
           array = tile.to_numpy()
           
           # Convert to PyTorch tensor
           tensor = torch.from_numpy(array).permute(2, 0, 1).float() / 255.0
           
           return tensor, (x, y)  # Return tensor and position
   
   # Usage
   dataset = FimageDataset("large_slide.tiff")
   loader = DataLoader(dataset, batch_size=32, num_workers=4)
   
   for batch, positions in loader:
       # batch shape: (32, 3, 224, 224)
       # Train your model...
       pass

TensorFlow Dataset
~~~~~~~~~~~~~~~~~~

Using fimage with TensorFlow's Dataset API:

.. code-block:: python

   import fim
   import tensorflow as tf
   import numpy as np
   
   def create_tf_dataset(image_path, tile_size=224, stride=112):
       """Create TensorFlow Dataset from fimage."""
       
       def tile_generator():
           img = fim.Image.from_libtiff(image_path)
           width, height, channels = img.dimensions
           
           for y in range(0, height - tile_size + 1, stride):
               for x in range(0, width - tile_size + 1, stride):
                   tile = img.crop(x, y, tile_size, tile_size)
                   array = tile.to_numpy().astype(np.float32) / 255.0
                   yield array
       
       dataset = tf.data.Dataset.from_generator(
           tile_generator,
           output_signature=tf.TensorSpec(
               shape=(tile_size, tile_size, 3), 
               dtype=tf.float32
           )
       )
       
       return dataset.batch(32).prefetch(tf.data.AUTOTUNE)
   
   # Usage
   dataset = create_tf_dataset("large_slide.tiff")
   
   for batch in dataset:
       # batch shape: (32, 224, 224, 3)
       # Train your model...
       pass

Performance Optimization Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Efficient processing of very large images:

.. code-block:: python

   import fim
   import multiprocessing as mp
   
   def process_tile(args):
       """Process a single tile (worker function)."""
       image_path, x, y, tile_size = args
       
       # Each worker loads the image independently
       img = fim.Image.from_libtiff(image_path)
       tile = img.crop(x, y, tile_size, tile_size)
       array = tile.to_numpy()
       
       # Perform expensive computation
       # ... custom processing ...
       
       return (x, y), result
   
   def process_large_image_parallel(image_path, tile_size=1024):
       """Process large image in parallel using multiprocessing."""
       
       # Load image to get dimensions
       img = fim.Image.from_libtiff(image_path)
       width, height, channels = img.dimensions
       
       # Generate tile positions
       tile_positions = []
       for y in range(0, height, tile_size):
           for x in range(0, width, tile_size):
               tile_positions.append((image_path, x, y, tile_size))
       
       # Process tiles in parallel
       with mp.Pool(mp.cpu_count()) as pool:
           results = pool.map(process_tile, tile_positions)
       
       # Assemble results
       # ...
       
       return results

Common Patterns
---------------

Error Handling
~~~~~~~~~~~~~~

.. code-block:: python

   def safe_image_processing(input_path, output_path):
       """Robust image processing with error handling."""
       try:
           img = fim.Image.from_libtiff(input_path)
       except RuntimeError as e:
           print(f"Failed to load {input_path}: {e}")
           return False
       
       try:
           result = img.crop(0, 0, 512, 512).downsample(2)
           result.write_png(output_path)
       except Exception as e:
           print(f"Processing failed: {e}")
           return False
       
       return True

Batch Processing
~~~~~~~~~~~~~~~~

.. code-block:: python

   from pathlib import Path
   
   def batch_process_directory(input_dir, output_dir):
       """Process all TIFF files in a directory."""
       input_dir = Path(input_dir)
       output_dir = Path(output_dir)
       output_dir.mkdir(exist_ok=True)
       
       for tiff_file in input_dir.glob("*.tiff"):
           print(f"Processing {tiff_file.name}...")
           
           img = fim.Image.from_libtiff(str(tiff_file))
           result = img.crop(0, 0, 1024, 1024).downsample(4)
           
           output_file = output_dir / f"{tiff_file.stem}_processed.png"
           result.write_png(str(output_file))

Next Steps
----------

- Modify examples for your use case
- Explore :doc:`../guides/index` for creating custom components
- See :doc:`../api/index` for detailed API reference


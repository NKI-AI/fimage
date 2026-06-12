Python API Reference
====================

The fimage Python API provides a high-level, NumPy-integrated interface to the fimage 
C++ library with lazy evaluation and zero-copy operations.

.. currentmodule:: fim

Installation & Import
---------------------

.. code-block:: python

   import fim
   import numpy as np
   from pathlib import Path

Core Classes
------------

Image
~~~~~

.. autoclass:: fim.Image
   :members:
   :inherited-members:
   :special-members: __init__
   
   The main image class that wraps the CRTP pipeline with type erasure. This provides
   a clean Python API while maintaining lazy evaluation and zero-copy operations.
   
   **Creation Methods:**
   
   - `from_libtiff(filename)`: Load image from TIFF file
   - `from_png(filename)`: Load image from PNG file
   - `from_fastslide(filename, level=0)`: Load whole slide image at specific pyramid level
   - `from_memory(data, width, height, channels)`: Create from buffer
   - `stack(images, axis)`: Stack multiple images
   
   **Pipeline Methods:**
   
   - `crop(x, y, width, height)`: Extract rectangular region
   - `downsample(factor)`: Downsample by integer factor
   - `resize(width, height, kernel, box)`: Resize to target dimensions with high-quality resampling
   
   **Output Methods:**
   
   - `write_tiff(filename)`: Save to TIFF file
   - `write_png(filename)`: Save to PNG file
   - `to_numpy()`: Convert to NumPy array
   
   **Query Methods:**
   
   - `dimensions`: Get (width, height, channels)

.. note::
   All pipeline operations (`crop`, `downsample`, `resize`) return a new `Image` instance and 
   use lazy evaluation. The computation happens when you call an output method 
   (`write_tiff`, `write_png`, `to_numpy`).

Enumerations and Utility Classes
---------------------------------

KernelType
~~~~~~~~~~

.. py:class:: fim.KernelType

   Enumeration of available resampling kernels for the resize operator.
   
   .. py:attribute:: LANCZOS2
   
      Lanczos kernel with radius 2. Faster but slightly less sharp than Lanczos3.
   
   .. py:attribute:: LANCZOS3
   
      Lanczos kernel with radius 3 (default). Good balance of quality and speed.
   
   .. py:attribute:: MAGIC2021
   
      Magic Kernel Sharp 2021 with radius 4.5. Optimized for sharp, high-quality 
      resampling with minimal ringing artifacts. Originally designed for subpixel 
      shifts in whole slide imaging.

Box
~~~

.. py:class:: fim.Box(x1, y1, x2, y2)

   Specifies a source region with floating-point pixel coordinates for 
   subpixel-accurate cropping and resizing.
   
   :param float x1: Left edge in pixel coordinates
   :param float y1: Top edge in pixel coordinates  
   :param float x2: Right edge in pixel coordinates
   :param float y2: Bottom edge in pixel coordinates
   
   Example:
   
   .. code-block:: python
   
      # Create box from pixel 10.5 to 110.5 in both dimensions
      box = fim.Box(10.5, 10.5, 110.5, 110.5)
      
      # Use with resize for subpixel-accurate cropping
      resized = img.resize(100, 100, box=box)

Python API Examples
-------------------

Basic Image Loading and Processing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   
   # Load image (lazy - file opened but not fully read)
   img = fim.Image.from_libtiff("input.tiff")
   
   # Get image information
   width, height, channels = img.dimensions
   print(f"Image: {width}x{height}, {channels} channels")
   
   # Apply operations (lazy - computation graph built)
   result = img.crop(100, 100, 512, 512).downsample(2)
   
   # Materialize result (evaluation happens here)
   result.write_png("output.png")

Lazy Evaluation Demonstration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   import time
   
   # Build pipeline (instant - no computation)
   start = time.time()
   img = fim.Image.from_libtiff("large_image.tiff")
   cropped = img.crop(0, 0, 2048, 2048)
   downsampled = cropped.downsample(4)
   print(f"Pipeline construction: {time.time() - start:.3f}s")  # ~0.000s
   
   # Trigger evaluation (this is when work happens)
   start = time.time()
   array = downsampled.to_numpy()
   print(f"Evaluation: {time.time() - start:.3f}s")  # Actual processing time
   print(f"Output shape: {array.shape}")

NumPy Integration
~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   import numpy as np
   import matplotlib.pyplot as plt
   
   # Load image
   img = fim.Image.from_libtiff("input.tiff")
   
   # Convert to NumPy (zero-copy when possible)
   array = img.to_numpy()
   print(f"NumPy array shape: {array.shape}")
   print(f"NumPy array dtype: {array.dtype}")
   
   # Process with NumPy
   normalized = array.astype(np.float32) / 255.0
   
   # Display with matplotlib
   plt.figure(figsize=(10, 10))
   plt.imshow(normalized)
   plt.axis('off')
   plt.title("Loaded Image")
   plt.show()

Creating Images from NumPy Arrays
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   import numpy as np
   
   # Create NumPy array
   width, height, channels = 512, 512, 3
   data = np.random.randint(0, 255, (height, width, channels), dtype=np.uint8)
   
   # Convert to fimage (note: expects flattened list)
   img = fim.Image.from_memory(
       data.flatten().tolist(),
       width,
       height,
       channels
   )
   
   # Process and save
   img.downsample(2).write_png("random_image.png")

Stacking Images
~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   
   # Load individual channel images
   red = fim.Image.from_png("red_channel.png")
   green = fim.Image.from_png("green_channel.png")
   blue = fim.Image.from_png("blue_channel.png")
   
   # Stack into RGB image
   rgb = fim.Image.stack([red, green, blue], axis="bands")
   
   # Verify dimensions
   width, height, channels = rgb.dimensions
   print(f"Stacked image: {width}x{height}, {channels} channels")
   
   # Process and save
   rgb.crop(0, 0, 1024, 1024).write_tiff("rgb_composite.tiff")

High-Quality Image Resizing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   
   # Load image
   img = fim.Image.from_libtiff("input.tiff")
   
   # Basic resize to target dimensions (default: Lanczos3)
   resized = img.resize(512, 512)
   resized.write_png("output_512x512.png")
   
   # Resize with different kernels
   lanczos2 = img.resize(1024, 1024, kernel=fim.KernelType.LANCZOS2)
   lanczos3 = img.resize(1024, 1024, kernel=fim.KernelType.LANCZOS3)
   magic = img.resize(1024, 1024, kernel=fim.KernelType.MAGIC2021)
   
   # Resize with subpixel-accurate box for precise cropping
   # Box specifies source region: (x1, y1, x2, y2) in pixel coordinates
   box = fim.Box(10.5, 20.5, 510.5, 520.5)
   cropped_resized = img.resize(256, 256, box=box)
   
   # Combined crop and resize with subpixel precision
   # This is more accurate than crop followed by resize
   box = fim.Box(100.25, 200.75, 600.25, 700.75)
   result = img.resize(500, 500, kernel=fim.KernelType.MAGIC2021, box=box)
   result.write_tiff("precise_crop_resize.tiff")

Multi-Resolution Processing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   
   # Load high-resolution image
   img = fim.Image.from_libtiff("slide.tiff")
   
   # Extract same region at multiple resolutions using downsample
   patch = img.crop(10000, 10000, 2048, 2048)
   patch.write_tiff("patch_full.tiff")
   patch.downsample(2).write_tiff("patch_half.tiff")
   patch.downsample(4).write_tiff("patch_quarter.tiff")
   
   # Or use resize for arbitrary output sizes
   patch.resize(1024, 1024).write_tiff("patch_1024.tiff")
   patch.resize(768, 768).write_tiff("patch_768.tiff")
   patch.resize(512, 512).write_tiff("patch_512.tiff")

Deep Learning Integration (PyTorch)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   import torch
   from torch.utils.data import Dataset, DataLoader
   
   class TileDataset(Dataset):
       def __init__(self, image_path, tile_size=224, stride=224):
           self.image = fim.Image.from_libtiff(image_path)
           self.tile_size = tile_size
           self.stride = stride
           
           # Get image dimensions
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
           
           # Extract tile (lazy evaluation)
           tile = self.image.crop(x, y, self.tile_size, self.tile_size)
           
           # Convert to NumPy then PyTorch tensor
           array = tile.to_numpy()
           tensor = torch.from_numpy(array).permute(2, 0, 1).float() / 255.0
           
           return tensor
   
   # Use with PyTorch DataLoader
   dataset = TileDataset("large_image.tiff", tile_size=224, stride=112)
   loader = DataLoader(dataset, batch_size=32, num_workers=4, shuffle=True)
   
   # Training loop
   for batch in loader:
       # batch shape: (32, 3, 224, 224)
       # ... training code ...
       pass

Deep Learning Integration (TensorFlow)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   import tensorflow as tf
   import numpy as np
   
   def tile_generator(image_path, tile_size=224, stride=224):
       """Generator for TensorFlow Dataset."""
       img = fim.Image.from_libtiff(image_path)
       width, height, channels = img.dimensions
       
       for y in range(0, height - tile_size + 1, stride):
           for x in range(0, width - tile_size + 1, stride):
               tile = img.crop(x, y, tile_size, tile_size)
               array = tile.to_numpy().astype(np.float32) / 255.0
               yield array
   
   # Create TensorFlow dataset
   dataset = tf.data.Dataset.from_generator(
       lambda: tile_generator("large_image.tiff"),
       output_signature=tf.TensorSpec(shape=(224, 224, 3), dtype=tf.float32)
   )
   
   # Batch and prefetch
   dataset = dataset.batch(32).prefetch(tf.data.AUTOTUNE)
   
   # Use in training
   for batch in dataset:
       # batch shape: (32, 224, 224, 3)
       # ... training code ...
       pass

Batch Processing
~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   from pathlib import Path
   
   def process_images(input_dir, output_dir, crop_size=512, downsample=2):
       """Process all TIFF files in a directory."""
       input_dir = Path(input_dir)
       output_dir = Path(output_dir)
       output_dir.mkdir(exist_ok=True)
       
       for tiff_file in input_dir.glob("*.tiff"):
           print(f"Processing {tiff_file.name}...")
           
           # Load and process
           img = fim.Image.from_libtiff(str(tiff_file))
           result = img.crop(0, 0, crop_size, crop_size).downsample(downsample)
           
           # Save with same name
           output_file = output_dir / tiff_file.name
           result.write_tiff(str(output_file))
   
   # Process all images
   process_images("input_images/", "output_images/", crop_size=1024, downsample=4)

Error Handling
~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   
   def safe_load_image(filename):
       """Load image with error handling."""
       try:
           img = fim.Image.from_libtiff(filename)
           width, height, channels = img.dimensions
           print(f"Loaded {filename}: {width}x{height}, {channels} channels")
           return img
       except RuntimeError as e:
           print(f"Failed to load {filename}: {e}")
           return None
       except Exception as e:
           print(f"Unexpected error loading {filename}: {e}")
           return None
   
   # Try to load various files
   for filename in ["good.tiff", "missing.tiff", "corrupted.tiff"]:
       img = safe_load_image(filename)
       if img is not None:
           # Process image...
           pass

Memory-Efficient Processing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   
   def process_large_image_in_tiles(input_path, output_path, tile_size=1024):
       """Process a large image tile by tile to minimize memory usage."""
       img = fim.Image.from_libtiff(input_path)
       width, height, channels = img.dimensions
       
       print(f"Processing {width}x{height} image in {tile_size}x{tile_size} tiles")
       
       # Process each tile independently
       for y in range(0, height, tile_size):
           for x in range(0, width, tile_size):
               # Extract and process tile (lazy - only this tile in memory)
               tile = img.crop(x, y, tile_size, tile_size)
               
               # Convert to NumPy for processing
               array = tile.to_numpy()
               
               # Process array...
               # (e.g., apply filters, detect features, etc.)
               
               # Tile goes out of scope, memory freed
       
       print("Processing complete")

Performance Tips
----------------

1. **Use Lazy Evaluation**
   
   Build your entire pipeline before materializing the result:

   .. code-block:: python

      # Good: Single evaluation
      result = img.crop(...).downsample(...).to_numpy()
      
      # Bad: Multiple evaluations
      cropped = img.crop(...)
      cropped_array = cropped.to_numpy()  # Evaluation 1
      downsampled = fim.Image.from_memory(...).downsample(...)
      result = downsampled.to_numpy()  # Evaluation 2

2. **Minimize Conversions**
   
   Avoid unnecessary conversions between fimage and NumPy:

   .. code-block:: python

      # Good: Process in fimage, convert once
      result = img.crop(...).downsample(...).to_numpy()
      
      # Bad: Multiple conversions
      array1 = img.to_numpy()
      cropped = array1[100:612, 100:612]
      # Convert back to fimage...

3. **Tile Size Awareness**
   
   For TIFF files, crop sizes aligned with internal tile size are most efficient:

   .. code-block:: python

      # Check ideal tile size (implementation detail, not exposed in Python yet)
      # Generally, use multiples of 256 or 512 for TIFF files
      
      # Good: Aligned with common TIFF tile size
      tile = img.crop(0, 0, 512, 512)
      
      # Less efficient: Misaligned
      tile = img.crop(0, 0, 500, 500)

4. **Batch Operations**
   
   When processing multiple regions, reuse the source image:

   .. code-block:: python

      # Good: Single source, multiple crops
      img = fim.Image.from_libtiff("large.tiff")
      tile1 = img.crop(0, 0, 512, 512).to_numpy()
      tile2 = img.crop(512, 0, 512, 512).to_numpy()
      
      # Bad: Reload source each time
      tile1 = fim.Image.from_libtiff("large.tiff").crop(0, 0, 512, 512).to_numpy()
      tile2 = fim.Image.from_libtiff("large.tiff").crop(512, 0, 512, 512).to_numpy()

Comparison with Other Libraries
--------------------------------

fimage vs PIL/Pillow
~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   # PIL/Pillow
   from PIL import Image
   img = Image.open("input.tiff")
   cropped = img.crop((100, 100, 612, 612))  # Load entire image first
   cropped.save("output.png")
   
   # fimage (lazy evaluation)
   import fim
   fim.Image.from_libtiff("input.tiff").crop(100, 100, 512, 512).write_png("output.png")

fimage vs OpenSlide
~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   # OpenSlide (for whole-slide images)
   import openslide
   slide = openslide.OpenSlide("slide.svs")
   region = slide.read_region((100, 100), 0, (512, 512))
   
   # fimage (generic tile-based processing)
   import fim
   region = fim.Image.from_libtiff("slide.tiff").crop(100, 100, 512, 512).to_numpy()

fimage vs PyVips
~~~~~~~~~~~~~~~~

.. code-block:: python

   # PyVips (similar lazy evaluation approach)
   import pyvips
   img = pyvips.Image.new_from_file("input.tiff")
   result = img.crop(100, 100, 512, 512).resize(0.5)
   result.write_to_file("output.png")
   
   # fimage (inspired by PyVips design)
   import fim
   fim.Image.from_libtiff("input.tiff").crop(100, 100, 512, 512).downsample(2).write_png("output.png")


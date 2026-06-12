FImage Format Specification
============================

The FImage format is a custom binary image format designed for efficient storage and random access to large images. It provides support for tiling, compression, and metadata preservation while maintaining simplicity and performance.

.. contents::
   :local:
   :depth: 2

Overview
--------

**Key Features:**

- Fixed 256-byte header for fast metadata access
- Optional tiling for efficient random access to large images
- Multiple compression algorithms (None, LZ4, Zstd)
- Per-tile compression for parallel processing
- Metadata preservation (MPP, pixel type, layout)
- Efficient seek table for compressed data
- Little-endian format for cross-platform compatibility

**Design Goals:**

1. **Fast Random Access**: Tiled storage with seek table enables efficient region extraction
2. **Compression Flexibility**: Support multiple compression algorithms with per-tile granularity
3. **Metadata Rich**: Preserve important image metadata like microns-per-pixel
4. **Simple Implementation**: Straightforward binary format without complex dependencies
5. **Streaming Friendly**: Single-pass writing with seek table at end

File Structure
--------------

The FImage file consists of three main sections:

.. code-block:: text

   ┌──────────────────────┐
   │  Header (256 bytes)  │  Magic, dimensions, metadata
   ├──────────────────────┤
   │                      │
   │   Tile/Image Data    │  Compressed or uncompressed pixel data
   │   (variable size)    │
   │                      │
   ├──────────────────────┤
   │   Seek Table         │  Optional: offsets for compressed tiles
   │   (if compressed)    │
   └──────────────────────┘

Header Format
-------------

The header is exactly **256 bytes** and contains all metadata needed to interpret the file:

.. list-table:: FImage Header Layout
   :widths: 15 10 10 65
   :header-rows: 1

   * - Field
     - Offset
     - Size
     - Description
   * - ``magic``
     - 0
     - 8 bytes
     - Magic signature: ``"FIMAGE\x01\x00"`` (includes version 1)
   * - ``width``
     - 8
     - 4 bytes
     - Image width in pixels (uint32_t, little-endian)
   * - ``height``
     - 12
     - 4 bytes
     - Image height in pixels (uint32_t, little-endian)
   * - ``channels``
     - 16
     - 4 bytes
     - Number of channels (uint32_t, little-endian)
   * - ``pixel_type``
     - 20
     - 1 byte
     - Pixel data type (see `Pixel Types`_)
   * - ``data_layout``
     - 21
     - 1 byte
     - Memory layout: 0=HWC (channels-last), 1=CHW (channels-first)
   * - ``compression``
     - 22
     - 1 byte
     - Compression type (see `Compression Types`_)
   * - ``quantization_mode``
     - 23
     - 1 byte
     - Reserved for future use (always 0)
   * - ``tile_width``
     - 24
     - 4 bytes
     - Tile width in pixels (0 = contiguous mode)
   * - ``tile_height``
     - 28
     - 4 bytes
     - Tile height in pixels (0 = contiguous mode)
   * - ``num_tiles_x``
     - 32
     - 4 bytes
     - Number of tiles horizontally (0 in contiguous mode)
   * - ``num_tiles_y``
     - 36
     - 4 bytes
     - Number of tiles vertically (0 in contiguous mode)
   * - ``mpp_x``
     - 40
     - 8 bytes
     - Microns per pixel in X direction (double, 0.0 = unknown)
   * - ``mpp_y``
     - 48
     - 8 bytes
     - Microns per pixel in Y direction (double, 0.0 = unknown)
   * - ``data_offset``
     - 56
     - 8 bytes
     - Offset to tile data (always 256)
   * - ``seek_table_offset``
     - 64
     - 8 bytes
     - Offset to seek table (0 if uncompressed)
   * - ``reserved``
     - 72
     - 184 bytes
     - Reserved for future extensions (all zeros)

Pixel Types
~~~~~~~~~~~

The ``pixel_type`` field indicates the data type of each pixel component:

.. list-table:: Supported Pixel Types
   :widths: 10 20 70
   :header-rows: 1

   * - Value
     - Type
     - Description
   * - 0
     - ``kUInt8``
     - Unsigned 8-bit integer (0-255)
   * - 1
     - ``kUInt16``
     - Unsigned 16-bit integer
   * - 2
     - ``kFloat32``
     - 32-bit floating point (IEEE 754 single precision)

Compression Types
~~~~~~~~~~~~~~~~~

The ``compression`` field specifies the compression algorithm:

.. list-table:: Compression Algorithms
   :widths: 10 20 70
   :header-rows: 1

   * - Value
     - Type
     - Description
   * - 0
     - ``kNone``
     - No compression (raw pixel data)
   * - 1
     - ``kLZ4``
     - LZ4 compression (fast, moderate ratio)
   * - 2
     - ``kZstd``
     - Zstandard compression (slower, better ratio)

Storage Modes
-------------

FImage supports two storage modes: **Contiguous** and **Tiled**.

Contiguous Mode
~~~~~~~~~~~~~~~

In contiguous mode, the entire image is stored as a single block of data.

**When to use:**

- Small to medium images that fit in memory
- Sequential reading of entire image
- Simple streaming scenarios

**Header values:**

- ``tile_width = 0``
- ``tile_height = 0``
- ``num_tiles_x = 0``
- ``num_tiles_y = 0``

**Data layout:**

.. code-block:: text

   ┌──────────────────────┐
   │  Header (256 bytes)  │
   ├──────────────────────┤
   │                      │
   │  Full Image Data     │  width × height × channels bytes
   │  (uncompressed)      │  or
   │                      │  compressed block
   │                      │
   ├──────────────────────┤
   │  Seek Table          │  (only if compressed, single entry)
   └──────────────────────┘

**Uncompressed size:**

.. code-block:: text

   size = width × height × channels × pixel_size

**Compressed layout:**

If compressed, a single seek table entry is appended:

- ``offset``: File offset to compressed data (usually 256)
- ``compressed_size``: Size of compressed block

Tiled Mode
~~~~~~~~~~

In tiled mode, the image is divided into rectangular tiles that can be read independently.

**When to use:**

- Large images (gigapixel whole slide images)
- Random access to image regions
- Parallel processing
- Progressive loading

**Header values:**

- ``tile_width > 0`` (e.g., 256)
- ``tile_height > 0`` (e.g., 256)
- ``num_tiles_x = ceil(width / tile_width)``
- ``num_tiles_y = ceil(height / tile_height)``

**Tile layout:**

Tiles are stored in **row-major order** (left-to-right, top-to-bottom):

.. code-block:: text

   Image:                   Storage Order:
   ┌─────┬─────┬─────┐      Tile 0, Tile 1, Tile 2,
   │  0  │  1  │  2  │      Tile 3, Tile 4, Tile 5,
   ├─────┼─────┼─────┤      Tile 6, Tile 7, Tile 8
   │  3  │  4  │  5  │
   ├─────┼─────┼─────┤
   │  6  │  7  │  8  │
   └─────┴─────┴─────┘

**Edge tiles:**

Tiles at the right and bottom edges are **padded** to full tile size if the image dimensions are not exact multiples of the tile size. Padding is typically zeros.

**Tile size:**

.. code-block:: text

   tile_bytes = tile_width × tile_height × channels × pixel_size

Seek Table
----------

For compressed data, a seek table is appended at the end of the file. Each entry contains the file offset and compressed size of a tile (or the entire image in contiguous mode).

Seek Table Entry
~~~~~~~~~~~~~~~~

Each entry is **16 bytes**:

.. list-table:: Seek Table Entry Format
   :widths: 20 10 70
   :header-rows: 1

   * - Field
     - Size
     - Description
   * - ``offset``
     - 8 bytes
     - Absolute file offset to compressed tile data (uint64_t, little-endian)
   * - ``compressed_size``
     - 8 bytes
     - Size of compressed tile in bytes (uint64_t, little-endian)

Number of Entries
~~~~~~~~~~~~~~~~~

- **Contiguous mode**: 1 entry (entire image)
- **Tiled mode**: ``num_tiles_x × num_tiles_y`` entries

Entry Order
~~~~~~~~~~~

Entries are stored in the same row-major order as tiles:

.. code-block:: text

   Seek Table:
   ┌──────────────────────┐
   │ Entry 0 (Tile 0,0)   │  16 bytes
   ├──────────────────────┤
   │ Entry 1 (Tile 1,0)   │  16 bytes
   ├──────────────────────┤
   │ Entry 2 (Tile 2,0)   │  16 bytes
   ├──────────────────────┤
   │ ...                  │
   └──────────────────────┘

Reading Algorithm
~~~~~~~~~~~~~~~~~

To read a specific tile (x, y):

1. Calculate tile index: ``index = tile_y × num_tiles_x + tile_x``
2. Seek to seek table: ``file_offset = seek_table_offset + index × 16``
3. Read seek entry: ``(offset, compressed_size)``
4. Seek to tile data: ``file_offset = offset``
5. Read compressed data: ``read(compressed_size)``
6. Decompress to tile buffer

Example Files
-------------

Contiguous Uncompressed
~~~~~~~~~~~~~~~~~~~~~~~

Small 512×512 RGB image without compression:

.. code-block:: text

   Header (256 bytes):
   - magic: "FIMAGE\x01\x00"
   - width: 512
   - height: 512
   - channels: 3
   - pixel_type: 0 (uint8)
   - compression: 0 (None)
   - tile_width: 0
   - data_offset: 256
   - seek_table_offset: 0

   Data (786,432 bytes):
   - Raw RGB data: 512 × 512 × 3 = 786,432 bytes

   Total size: 256 + 786,432 = 786,688 bytes

Tiled Compressed
~~~~~~~~~~~~~~~~

Large 4096×4096 RGB image with 256×256 tiles and Zstd compression:

.. code-block:: text

   Header (256 bytes):
   - magic: "FIMAGE\x01\x00"
   - width: 4096
   - height: 4096
   - channels: 3
   - pixel_type: 0 (uint8)
   - compression: 2 (Zstd)
   - tile_width: 256
   - tile_height: 256
   - num_tiles_x: 16
   - num_tiles_y: 16
   - data_offset: 256
   - seek_table_offset: <calculated>

   Tile Data (variable size):
   - 256 compressed tiles (16×16)
   - Each tile: 256 × 256 × 3 = 196,608 bytes uncompressed
   - Compressed size varies per tile

   Seek Table (4,096 bytes):
   - 256 entries × 16 bytes = 4,096 bytes

Usage Examples
--------------

Writing FImage Files
~~~~~~~~~~~~~~~~~~~~

C++ Example
^^^^^^^^^^^

.. code-block:: cpp

   #include <fim/image.h>
   #include <fim/sinks/fimage_sink.h>

   // Write uncompressed contiguous image
   fim::Image<fim::TiffSource>("input.tiff")
       .Render(fim::FImageSink::Create("output.fimage"));

   // Write tiled with compression
   fim::Image<fim::TiffSource>("large_image.tiff")
       .Render(fim::FImageSink::Create(
           "output.fimage",
           fim::TileSize(512, 512),
           fim::CompressionType::kZstd));

   // Write with metadata
   fim::Image<fim::TiffSource>("slide.tiff")
       .Render(fim::FImageSink::Create(
           "slide.fimage",
           fim::TileSize(256, 256),
           fim::CompressionType::kLZ4,
           0.25,  // mpp_x (microns per pixel)
           0.25   // mpp_y
       ));

Python Example
^^^^^^^^^^^^^^

.. code-block:: python

   import fim

   # Write uncompressed
   img = fim.Image.from_libtiff("input.tiff")
   img.write_fimage("output.fimage")

   # Write tiled with compression
   img = fim.Image.from_libtiff("large_image.tiff")
   img.write_fimage(
       "output.fimage",
       tile_size=(512, 512),
       compression="zstd"
   )

   # Write with metadata
   img = fim.Image.from_libtiff("slide.tiff")
   img.write_fimage(
       "slide.fimage",
       tile_size=(256, 256),
       compression="lz4",
       mpp=(0.25, 0.25)
   )

Reading FImage Files
~~~~~~~~~~~~~~~~~~~~

C++ Example
^^^^^^^^^^^

.. code-block:: cpp

   #include <fim/image.h>
   #include <fim/sources/fimage_source.h>

   // Read entire image
   fim::Image<fim::FImageSource>("input.fimage")
       .Render(fim::TiffSink::Create("output.tiff"));

   // Read specific region (efficient with tiled files)
   fim::Image<fim::FImageSource>("large.fimage")
       .Crop(1000, 1000, 512, 512)
       .Render(fim::TiffSink::Create("region.tiff"));

Python Example
^^^^^^^^^^^^^^

.. code-block:: python

   import fim

   # Read entire image
   img = fim.Image.from_fimage("input.fimage")
   array = img.to_numpy()

   # Read specific region (efficient with tiled files)
   img = fim.Image.from_fimage("large.fimage")
   region = img.crop(1000, 1000, 512, 512).to_numpy()

   # Access metadata
   img = fim.Image.from_fimage("slide.fimage")
   mpp = img.get_mpp()  # Returns (mpp_x, mpp_y)
   dims = img.dimensions

Implementation Notes
--------------------

Byte Order
~~~~~~~~~~

All multi-byte integers and floating-point values are stored in **little-endian** format. This is the native byte order on most modern systems (x86, ARM).

Alignment
~~~~~~~~~

The 256-byte header is cache-line friendly (typically 64 bytes per cache line). No special alignment is required for tile data.

Padding
~~~~~~~

Edge tiles are padded with zeros to full tile size. This simplifies implementation but slightly increases file size for compressed tiles.

Thread Safety
~~~~~~~~~~~~~

- **Writing**: Single-threaded (sequential tile writing)
- **Reading**: Thread-safe for concurrent tile reads (each thread gets tile independently)

Compression Considerations
~~~~~~~~~~~~~~~~~~~~~~~~~~

- **LZ4**: Fast compression/decompression, moderate compression ratio (2-3x typical)
- **Zstd**: Slower but better compression ratio (3-5x typical)
- **None**: No CPU overhead, largest files, fastest for SSD/NVMe storage

**Compression is applied per-tile**, enabling:

- Parallel compression during writing (future optimization)
- Independent tile decompression during reading
- Mixed compression within same file format (via seek table)

Limitations
-----------

Current Version (v1)
~~~~~~~~~~~~~~~~~~~~

- Only ``uint8`` pixel type fully supported
- No color space metadata (assumes RGB/grayscale)
- No support for pyramidal/multi-resolution images
- No support for extra metadata (EXIF, ICC profiles)
- Maximum image size: 4,294,967,295 × 4,294,967,295 pixels (uint32_t limit)

Future Enhancements
~~~~~~~~~~~~~~~~~~~

Planned for future versions:

- Multi-resolution pyramids (like TIFF)
- Additional pixel types (uint16, float32)
- Color space metadata
- Embedded thumbnails
- Custom metadata key-value pairs
- Checksums for data integrity
- Encryption support

Comparison with Other Formats
------------------------------

.. list-table:: Format Comparison
   :widths: 15 15 15 15 15 25
   :header-rows: 1

   * - Feature
     - FImage
     - TIFF
     - PNG
     - JPEG
     - BigTIFF
   * - **Tiling**
     - ✓
     - ✓
     - ✗
     - ✗
     - ✓
   * - **Compression**
     - LZ4, Zstd
     - Many
     - Deflate
     - Lossy
     - Many
   * - **Random Access**
     - ✓ (fast)
     - ✓ (slower)
     - ✗
     - ✗
     - ✓ (slower)
   * - **Simplicity**
     - Very High
     - Low
     - Medium
     - Medium
     - Low
   * - **File Size**
     - Medium
     - Variable
     - Small
     - Smallest
     - Variable
   * - **Metadata**
     - Basic
     - Rich
     - Basic
     - Basic
     - Rich
   * - **Use Case**
     - Fast I/O
     - General
     - Web
     - Photos
     - Large images

**When to use FImage:**

- Need fast random access to large images
- Want simple, maintainable code
- Require efficient tiled storage
- Don't need extensive metadata
- Prioritize performance over file size

**When to use TIFF instead:**

- Need rich metadata support
- Require multiple compression options
- Need pyramidal multi-resolution
- Interoperability with other tools

References
----------

**Related Documentation:**

- :doc:`adding_source` - Implementing FImageSource
- :doc:`adding_sink` - Implementing FImageSink
- :doc:`../api/cpp_api` - C++ API reference

**Source Code:**

- ``include/fim/sinks/fimage_sink.h`` - FImage writer implementation
- ``include/fim/sources/fimage_source.h`` - FImage reader implementation
- ``include/fim/utilities/compressor.h`` - Compression utilities
- ``include/fim/utilities/decompressor.h`` - Decompression utilities

**External Resources:**

- `LZ4 <https://lz4.org/>`_ - LZ4 compression algorithm
- `Zstandard <https://facebook.github.io/zstd/>`_ - Zstandard compression
- `TIFF Specification <https://www.awaresystems.be/imaging/tiff.html>`_ - TIFF format reference


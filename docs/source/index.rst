fimage Documentation
====================

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   overview
   architecture
   api/index
   guides/index
   examples/index

Documentation Structure
-----------------------

fimage provides two complementary documentation systems:

**This Sphinx Documentation** (You are here)
   - **Overview and Architecture**: High-level design, CRTP patterns, and lazy evaluation
   - **FImage Format**: Complete specification of the native file format
   - **Python API**: Complete Python interface documentation
   - **C++ API Overview**: Quick reference with Breathe integration
   - **Extension Guides**: Step-by-step tutorials for adding sources, operators, and sinks

**`Doxygen Documentation <../doxygen/index.html>`_** (Detailed C++ Reference)
   - **Interactive Diagrams**: Zoomable SVG dependency graphs
   - **Complete C++ API**: Every class, function, and namespace
   - **Call Graphs**: Function dependencies and relationships
   - **Inheritance Diagrams**: Visual class hierarchies
   - **Source Browser**: Browse C++ source code with syntax highlighting

.. tip::
   **For C++ Developers**: Start with the `Doxygen Documentation <../doxygen/index.html>`_ 
   for comprehensive API details and interactive visualizations.
   
   **For Python Users**: Continue with the :doc:`api/python_api` for complete Python interface docs.
   
   **For New Contributors**: See the :doc:`guides/index` for step-by-step implementation guides.

.. image:: https://img.shields.io/badge/version-0.1.2-blue.svg
   :alt: Version 0.1.2

.. image:: https://img.shields.io/badge/C++-20-green.svg
   :alt: C++20

.. image:: https://img.shields.io/badge/Python-3.11+-green.svg
   :alt: Python 3.11+

fimage (FastImage) is a minimal C++20 library for tile-based image processing using CRTP 
(Curiously Recurring Template Pattern) with lazy evaluation and Python bindings.

Features
--------

🚀 **High Performance**
   - Zero-overhead CRTP pipeline composition
   - Compile-time polymorphism
   - Lazy evaluation (operations deferred until materialization)
   - Multi-threaded tile processing

🔧 **Flexible Pipeline**
   - Tile-based processing for large images
   - Chainable fluent API
   - Multiple sources (TIFF, PNG, FImage, memory)
   - Extensible operators (crop, downsample, stack)
   - Flexible sinks (TIFF, PNG, FImage, NumPy arrays)
   - Native FImage format with tiling and compression

🌐 **Dual APIs**
   - Native C++20 API with modern features
   - Python bindings with NumPy integration
   - PyVips-style API for familiarity
   - Zero-copy NumPy conversion

🐍 **Python Integration**
   - Type-erased pipeline for clean Python API
   - Lazy evaluation preserved
   - NumPy array support
   - Easy integration with PyTorch/TensorFlow

Quick Start
-----------

C++ API
~~~~~~~

.. code-block:: cpp

   #include <fim/image.h>
   
   int main() {
       // Create a chained pipeline (lazy - nothing computed yet)
       fim::Image<fim::TiffSource>("input.tiff")
           .Crop(100, 100, 512, 512)
           .Downsample(2)
           .Render(fim::LodePngSink("output.png"));  // Evaluation happens here
       
       return 0;
   }

Python API
~~~~~~~~~~

.. code-block:: python

   import fim
   
   # Load image and apply operations (lazy - nothing computed yet)
   img = fim.Image.from_libtiff("input.tiff")
   result = img.crop(100, 100, 512, 512).downsample(2)
   
   # Trigger evaluation by writing to file
   result.write_png("output.png")
   
   # Or convert to NumPy array (evaluation happens here)
   array = result.to_numpy()  # shape: (height, width, channels)

Design Principles
-----------------

1. **Zero-overhead abstraction**: CRTP ensures no virtual function calls
2. **Lazy evaluation**: Operations are deferred until a sink materializes the result
3. **Tile-based processing**: Efficient memory usage for large images
4. **Composable pipeline**: Operators can be chained in any order
5. **Type safety**: Compile-time type checking for pipeline stages (C++)
6. **Type erasure for Python**: Stable ABI through concept/model pattern
7. **Extensible**: Easy to add new sources, operators, and sinks

Key Concepts
------------

**CRTP Pipeline**
   All pipeline stages use CRTP (Curiously Recurring Template Pattern) for zero-overhead 
   composition. This provides compile-time polymorphism without virtual function calls.

**Lazy Evaluation**
   Operations build a computation graph but don't execute until a sink materializes 
   the result. This allows optimization and prevents unnecessary computation.

**Tile-Based Processing**
   Images are processed in rectangular tiles, enabling efficient memory usage for 
   large images and natural parallelization.

**Type Erasure (Python)**
   Python bindings use type erasure to hide CRTP complexity while preserving 
   lazy evaluation and zero-copy operations.

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`


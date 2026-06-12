Extension Guides
================

This section provides step-by-step tutorials for extending fimage with new components.

.. toctree::
   :maxdepth: 2
   :caption: Guides:

   fimage_format
   adding_source
   adding_operator
   adding_sink
   python_bindings

Overview
--------

This section provides comprehensive guides for understanding and extending fimage.

**Format Specification:**

- **FImage Format**: Complete specification of the native FImage file format

**Component Types:**

fimage is designed to be extensible. You can add new components by inheriting from 
the appropriate CRTP base class and implementing the required methods:

- **Sources**: Provide image data from files, memory, or other sources
- **Operators**: Transform image data in the pipeline
- **Sinks**: Materialize the pipeline and produce output

Extension Architecture
----------------------

All components follow the CRTP (Curiously Recurring Template Pattern):

.. code-block:: cpp

   // Base class template
   template <typename Derived>
   class ComponentBase {
   public:
       // Methods that call derived implementation
       ReturnType Method() const {
           return static_cast<const Derived*>(this)->Method();
       }
   };
   
   // Derived component
   class MyComponent : public ComponentBase<MyComponent> {
   public:
       // Implement required methods
       ReturnType Method() const { /* implementation */ }
   };

This pattern provides:

- Zero-overhead abstraction (no virtual function calls)
- Compile-time polymorphism
- Type safety
- Inline optimization opportunities

Required Skills
---------------

To extend fimage, you should be familiar with:

**C++ Skills:**

- C++20 features (concepts, ranges, templates)
- CRTP pattern
- Move semantics and RAII
- Template metaprogramming (basic)

**Optional Skills:**

- pybind11 (for Python bindings)
- libtiff, lodepng, or other image libraries (for new formats)
- Multi-threading and synchronization

Development Workflow
--------------------

1. **Design**: Plan your component's interface and behavior
2. **Implement**: Create header and source files
3. **Test**: Write comprehensive unit tests
4. **Document**: Add Doxygen comments
5. **Integrate**: Update BUILD.bazel
6. **Python Bindings** (optional): Expose to Python API

Testing
~~~~~~~

All components should have comprehensive tests:

.. code-block:: cpp

   #include "fim/your_component.h"
   #include <gtest/gtest.h>
   
   TEST(YourComponentTest, BasicFunctionality) {
       // Test basic functionality
       YourComponent component(/* args */);
       auto result = component.Method();
       EXPECT_EQ(expected, result);
   }
   
   TEST(YourComponentTest, EdgeCases) {
       // Test edge cases
   }

Documentation
~~~~~~~~~~~~~

Use Doxygen-style comments:

.. code-block:: cpp

   /**
    * @brief Brief description of the class.
    *
    * Detailed description of what the class does, how it works,
    * and any important notes for users.
    *
    * @tparam T Template parameter description
    */
   template <typename T>
   class YourComponent {
   public:
       /**
        * @brief Brief method description.
        *
        * Detailed method description.
        *
        * @param param Parameter description
        * @return Return value description
        * @throw std::exception Exception conditions
        */
       ReturnType Method(ParamType param);
   };

Build Integration
~~~~~~~~~~~~~~~~~

Update `aifo/fimage/BUILD.bazel`:

.. code-block:: python

   cc_library(
       name = "fimage",
       srcs = glob([
           "src/**/*.cpp",
           "src/your_component.cpp",  # Add your source
       ]),
       hdrs = glob([
           "include/**/*.h",
           "include/fim/your_component.h",  # Add your header
       ]),
       # ... rest of configuration
   )
   
   cc_test(
       name = "your_component_test",
       srcs = ["src/your_component_test.cpp"],
       deps = [
           ":fimage",
           "@googletest//:gtest",
           "@googletest//:gtest_main",
       ],
   )

Quick Reference
---------------

.. list-table:: Component Quick Reference
   :widths: 20 40 40
   :header-rows: 1

   * - Component
     - Base Class
     - Required Methods
   * - **Source**
     - `SourceBase<Derived>`
     - `GetDimensions()`, `GetTile()`, `GetIdealTileSize()`
   * - **Operator**
     - `OperatorBase<Derived, InputType>`
     - `GetDimensions()`, `GetTile()`, `GetIdealTileSize()`
   * - **Sink**
     - `SinkBase<Derived>`
     - `Render(source)`

Next Steps
----------

**Learn the Format:**

- :doc:`fimage_format` - Understand the FImage file format specification

**Extend fimage:**

Choose a guide based on what you want to add:

- :doc:`adding_source` - Add support for a new image format
- :doc:`adding_operator` - Add a new image transformation
- :doc:`adding_sink` - Add a new output format
- :doc:`python_bindings` - Expose components to Python

Examples
--------

See the existing implementations for reference:

**Sources:**

- `include/fim/sources/tiff_source.h` - TIFF file source
- `include/fim/sources/png_source.h` - PNG file source
- `include/fim/sources/memory_source.h` - Memory buffer source

**Operators:**

- `include/fim/operators/crop.h` - Crop operator
- `include/fim/operators/downsample.h` - Downsample operator
- `include/fim/operators/stack.h` - Stack operator

**Sinks:**

- `include/fim/sinks/tiff_sink.h` - TIFF file sink
- `include/fim/sinks/lodepng_png_sink.h` - PNG file sink using lodepng
- `include/fim/sinks/memory_sink.h` - Memory buffer sink


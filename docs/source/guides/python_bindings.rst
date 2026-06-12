Adding Python Bindings
=======================

This guide walks you through exposing C++ components to the Python API using 
the type erasure pattern and pybind11.

Overview
--------

fimage uses a type erasure pattern to hide CRTP complexity from Python while 
maintaining lazy evaluation and zero-copy operations.

**Architecture:**

1. **StageConcept**: Abstract interface with virtual methods
2. **StageModel<T>**: Template wrapper implementing StageConcept for any CRTP type
3. **PyStage**: Handle class with `shared_ptr<StageConcept>`
4. **Factory functions**: Create PyStage instances from C++ components
5. **pybind11 bindings**: Expose PyStage and factories to Python

Prerequisites
-------------

- Understanding of fimage architecture (see :doc:`../architecture`)
- Basic knowledge of pybind11
- Familiarity with type erasure pattern

Adding a New Source to Python
------------------------------

Step 1: Create Factory Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add a factory function in `include/fim/python/factory.h`:

.. code-block:: cpp

   // include/fim/python/factory.h
   
   /**
    * @brief Creates a PyStage from a GradientSource.
    *
    * @param width Width of the gradient image
    * @param height Height of the gradient image
    * @param channels Number of channels
    * @return PyStage wrapping the GradientSource
    */
   inline PyStage CreateGradientStage(int width, int height, int channels = 3) {
       return PyStage(fim::GradientSource(width, height, channels));
   }

The PyStage constructor automatically wraps the source in StageModel.

Step 2: Add pybind11 Binding
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add the binding in `src/python/bindings.cpp`:

.. code-block:: cpp

   // src/python/bindings.cpp
   
   #include <pybind11/pybind11.h>
   #include "fim/python/factory.h"
   
   namespace py = pybind11;
   
   PYBIND11_MODULE(fim, m) {
       m.doc() = "fimage: Tile-based image processing with CRTP";
       
       // ... existing bindings ...
       
       // Add gradient source factory
       m.def("create_gradient", &CreateGradientStage,
             py::arg("width"),
             py::arg("height"),
             py::arg("channels") = 3,
             "Create a procedural gradient image source");
   }

Step 3: Add Python-Level API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add a classmethod to the Python Image class in `python/fim/__init__.py`:

.. code-block:: python

   # python/fim/__init__.py
   
   class Image:
       """High-level image processing interface."""
       
       @classmethod
       def from_gradient(cls, width: int, height: int, channels: int = 3) -> 'Image':
           """Create a gradient image.
           
           Args:
               width: Image width in pixels
               height: Image height in pixels
               channels: Number of channels (default: 3)
               
           Returns:
               Image instance with gradient source
               
           Example:
               >>> img = fim.Image.from_gradient(512, 512, 3)
               >>> img.write_png("gradient.png")
           """
           stage = _fim.create_gradient(width, height, channels)
           return cls(stage)

Step 4: Test Python Binding
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add tests in `src/python/gradient_test.py`:

.. code-block:: python

   import fim
   import numpy as np
   import pytest
   
   def test_gradient_creation():
       """Test gradient source creation."""
       img = fim.Image.from_gradient(512, 512, 3)
       assert img.dimensions == (512, 512, 3)
   
   def test_gradient_values():
       """Test gradient actually creates a gradient."""
       img = fim.Image.from_gradient(256, 256, 1)
       array = img.to_numpy()
       
       # Check shape
       assert array.shape == (256, 256, 1)
       
       # Check gradient (left should be dark, right should be light)
       assert array[0, 0, 0] < 10  # Left edge dark
       assert array[0, 255, 0] > 245  # Right edge light
       
       # Check gradient is smooth
       for x in range(255):
           assert array[0, x+1, 0] >= array[0, x, 0]
   
   def test_gradient_with_operations():
       """Test gradient source works in pipeline."""
       img = fim.Image.from_gradient(1024, 1024, 3)
       result = img.crop(256, 256, 512, 512).downsample(2)
       
       array = result.to_numpy()
       assert array.shape == (256, 256, 3)

Adding an Operator to Python
-----------------------------

**Overview:**

fimage uses a **reuse-based architecture** where Python bindings delegate to the 
same C++ CRTP operators used in the native API. This ensures:

- **Single source of truth**: Operators are implemented only once in C++
- **Consistent behavior**: Python and C++ use identical logic
- **Automatic improvements**: Bug fixes and optimizations apply to both APIs

Step 1: Implement C++ Operator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First, implement your operator following :doc:`adding_operator`. For example:

.. code-block:: cpp

   // include/fim/operators/rotate90.h
   
   template <typename InputType>
   class Rotate90 : public OperatorBase<Rotate90<InputType>, InputType> {
   public:
       explicit Rotate90(InputType input)
           : OperatorBase<Rotate90<InputType>, InputType>(std::move(input)) {}
       
       ImageInfo GetDimensions() const { /* ... */ }
       Tile GetTile(int x, int y, int width, int height) const { /* ... */ }
       TileSize GetIdealTileSize() const { /* ... */ }
   };

Step 2: Create Factory Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add a factory function in `include/fim/python/factory.h` that wraps the C++ operator:

.. code-block:: cpp

   // include/fim/python/factory.h
   
   #include "fim/operators/rotate90.h"
   #include "fim/python/stage_adapter.h"
   
   /**
    * @brief Applies a 90-degree rotation to a PyStage.
    *
    * This function reuses the C++ Rotate90 operator by wrapping
    * the type-erased stage in a PyStageAdapter.
    *
    * @param input Input PyStage to rotate
    * @return PyStage with rotation applied
    */
   inline PyStage ApplyRotate90(const PyStage& input) {
       // Adapt type-erased stage to CRTP interface
       PyStageAdapter adapter(input.GetStagePtr());
       
       // Apply C++ operator
       auto rotated = Rotate90<PyStageAdapter>(adapter);
       
       // Type-erase back to PyStage
       auto model = std::make_unique<StageModel<Rotate90<PyStageAdapter>>>(
           std::move(rotated)
       );
       return PyStage(std::move(model));
   }

**How it Works:**

1. **PyStageAdapter**: Wraps the type-erased `StageConcept` to provide the CRTP interface
2. **Rotate90<PyStageAdapter>**: C++ operator operates on the adapter
3. **StageModel**: Wraps the instantiated operator back into `StageConcept`
4. **PyStage**: Final handle returned to Python

Step 3: Add pybind11 Binding
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   // src/python/bindings.cpp
   
   // In PyStage class bindings
   py::class_<PyStage>(m, "Image")
       // ... existing methods ...
       .def("rotate90", &ApplyRotate90,
            "Rotate image 90 degrees clockwise");

Step 4: Test Operator Binding
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   import numpy as np
   
   def test_rotate90():
       """Test rotation operator."""
       # Create 2x2 test image
       data = [
           100, 0, 0,   200, 0, 0,  # Red and green in top row
           0, 0, 150,   250, 250, 0  # Blue and yellow in bottom row
       ]
       
       img = fim.Image.from_memory(data, 2, 2, 3)
       rotated = img.rotate90()
       
       # Check dimensions are swapped
       assert rotated.dimensions == (2, 2, 3)
       
       # Check pixels rotated correctly
       array = rotated.to_numpy()
       # After 90° CW rotation, bottom-left becomes top-left
       assert array[0, 0, 2] == 150  # Blue channel

Understanding PyStageAdapter
----------------------------

**The Problem:**

C++ operators are CRTP templates that require compile-time types:

.. code-block:: cpp

   template <typename InputType>
   class Crop : public OperatorBase<Crop<InputType>, InputType> {
       // Works with any InputType that has GetTile(), GetDimensions(), etc.
   };

Python bindings use type-erased `StageConcept` for runtime polymorphism:

.. code-block:: cpp

   class StageConcept {
   public:
       virtual ImageInfo GetDimensions() const = 0;
       virtual Tile GetTile(int x, int y, int width, int height) const = 0;
       // ...
   };

**The Solution: PyStageAdapter**

`PyStageAdapter` bridges these two worlds by providing a CRTP-compatible interface 
to type-erased stages:

.. code-block:: cpp

   // include/fim/python/stage_adapter.h
   
   class PyStageAdapter {
   public:
       explicit PyStageAdapter(std::shared_ptr<const StageConcept> stage)
           : stage_(std::move(stage)) {}
       
       // CRTP-compatible interface that forwards to type-erased stage
       ImageInfo GetDimensions() const {
           return stage_->GetDimensions();
       }
       
       Tile GetTile(int x, int y, int width, int height) const {
           return stage_->GetTile(x, y, width, height);
       }
       
       TileSize GetIdealTileSize() const {
           return stage_->GetIdealTileSize();
       }
   
   private:
       std::shared_ptr<const StageConcept> stage_;
   };

**Usage Pattern:**

.. code-block:: cpp

   // 1. Start with type-erased PyStage from Python
   PyStage input = ...;
   
   // 2. Wrap in adapter to get CRTP interface
   PyStageAdapter adapter(input.GetStagePtr());
   
   // 3. Pass to C++ operator template
   auto result = Crop<PyStageAdapter>(adapter, x, y, width, height);
   
   // 4. Type-erase back to PyStage
   auto model = std::make_unique<StageModel<Crop<PyStageAdapter>>>(
       std::move(result)
   );
   return PyStage(std::move(model));

**Benefits:**

- **No code duplication**: Python reuses C++ operators
- **Consistent behavior**: Same logic in both APIs
- **Automatic updates**: Bug fixes apply everywhere
- **Type safety**: Compile-time checks still apply
- **Performance**: Virtual calls only at stage boundaries

Real Example: Crop Operator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here's how the actual `ApplyCrop` factory function works:

.. code-block:: cpp

   // include/fim/python/factory.h
   
   inline PyStage ApplyCrop(const PyStage &input, int x, int y, int width,
                            int height) {
       // 1. Extract type-erased stage pointer
       //    GetStagePtr() returns shared_ptr<StageConcept>
       PyStageAdapter adapter(input.GetStagePtr());
       
       // 2. Instantiate C++ Crop operator with PyStageAdapter
       //    Type: Crop<PyStageAdapter>
       auto crop = Crop<PyStageAdapter>(adapter, x, y, width, height);
       
       // 3. Wrap in StageModel to type-erase back to StageConcept
       auto model = std::make_unique<StageModel<Crop<PyStageAdapter>>>(
           std::move(crop)
       );
       
       // 4. Return as PyStage handle
       return PyStage(std::move(model));
   }

**Flow Diagram:**

.. mermaid::

   graph LR
       A[PyStage Input] -->|GetStagePtr| B[shared_ptr&lt;StageConcept&gt;]
       B -->|PyStageAdapter| C[CRTP Interface]
       C -->|Crop&lt;PyStageAdapter&gt;| D[C++ Operator]
       D -->|StageModel| E[StageConcept]
       E -->|PyStage| F[PyStage Output]

Complex Example: Stack Operator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For operators taking multiple inputs, adapt each one:

.. code-block:: cpp

   inline PyStage ApplyStack(const std::vector<PyStage> &inputs,
                             const std::string &axis = "bands") {
       // Validate inputs
       if (inputs.empty()) {
           throw std::runtime_error("Cannot stack zero images");
       }
       
       // 1. Convert vector of PyStages to vector of adapters
       std::vector<PyStageAdapter> adapters;
       adapters.reserve(inputs.size());
       for (const auto &input : inputs) {
           adapters.emplace_back(input.GetStagePtr());
       }
       
       // 2. Create C++ Stack operator
       auto stack = Stack<PyStageAdapter>(std::move(adapters));
       
       // 3. Type-erase back to PyStage
       auto model = std::make_unique<StageModel<Stack<PyStageAdapter>>>(
           std::move(stack)
       );
       return PyStage(std::move(model));
   }

Adding a Sink to Python
-----------------------

Step 1: Add Render Method
~~~~~~~~~~~~~~~~~~~~~~~~~~

For sinks, add a method to PyStage that creates and uses the sink:

.. code-block:: cpp

   // include/fim/python/py_stage.h
   
   class PyStage {
   public:
       // ... existing methods ...
       
       /**
        * @brief Writes the image to a JPEG file.
        *
        * @param filename Output filename
        * @param quality JPEG quality (1-100)
        */
       void WriteJpeg(const std::string& filename, int quality = 90) const {
           CheckValid();
           
           // Create JPEG sink and render
           fim::JpegSink sink(filename, quality);
           sink.Render(*stage_);
       }
   };

Step 2: Add pybind11 Binding
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   // src/python/bindings.cpp
   
   py::class_<PyStage>(m, "Image")
       // ... existing methods ...
       .def("write_jpeg", &PyStage::WriteJpeg,
            py::arg("filename"),
            py::arg("quality") = 90,
            "Write image to JPEG file");

Step 3: Test Sink Binding
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   import fim
   import os
   
   def test_jpeg_write():
       """Test JPEG writing."""
       img = fim.Image.from_gradient(512, 512, 3)
       img.write_jpeg("test_output.jpg", quality=85)
       
       # Check file was created
       assert os.path.exists("test_output.jpg")
       assert os.path.getsize("test_output.jpg") > 0
       
       # Clean up
       os.remove("test_output.jpg")

Advanced Topics
---------------

Custom Return Types
~~~~~~~~~~~~~~~~~~~

If your operator returns custom data, you need to wrap it:

.. code-block:: cpp

   // Custom statistics structure
   struct ImageStats {
       double mean;
       double std_dev;
       uint8_t min_val;
       uint8_t max_val;
   };
   
   // Function that computes stats
   ImageStats ComputeStats(const PyStage& input) {
       auto stage_ptr = input.GetStagePtr();
       auto dims = stage_ptr->GetDimensions();
       auto tile_size = stage_ptr->GetIdealTileSize();
       
       // Compute statistics...
       
       return ImageStats{/* values */};
   }
   
   // Bind the structure and function
   PYBIND11_MODULE(fim, m) {
       py::class_<ImageStats>(m, "ImageStats")
           .def_readonly("mean", &ImageStats::mean)
           .def_readonly("std_dev", &ImageStats::std_dev)
           .def_readonly("min_val", &ImageStats::min_val)
           .def_readonly("max_val", &ImageStats::max_val);
       
       m.def("compute_stats", &ComputeStats, "Compute image statistics");
   }

NumPy Array Integration
~~~~~~~~~~~~~~~~~~~~~~~

For tighter NumPy integration, use pybind11's array handling:

.. code-block:: cpp

   #include <pybind11/numpy.h>
   
   py::array_t<uint8_t> ToNumpyArray(const PyStage& input) {
       auto stage_ptr = input.GetStagePtr();
       auto dims = stage_ptr->GetDimensions();
       
       // Create MemorySink
       fim::MemorySink sink(dims.width, dims.height, dims.channels);
       sink.Render(*stage_ptr);
       
       const auto& data = sink.GetDataAs<uint8_t>();
       
       // Create NumPy array (copy data)
       py::array_t<uint8_t> array({dims.height, dims.width, dims.channels});
       std::memcpy(array.mutable_data(), data.data(), data.size());
       
       return array;
   }

Callback Functions
~~~~~~~~~~~~~~~~~~

For progress reporting or custom processing:

.. code-block:: cpp

   void ProcessWithCallback(
       const PyStage& input,
       py::function callback  // Python callable
   ) {
       auto stage_ptr = input.GetStagePtr();
       auto dims = stage_ptr->GetDimensions();
       auto tile_size = stage_ptr->GetIdealTileSize();
       
       int total_tiles = ((dims.height + tile_size.height - 1) / tile_size.height) *
                        ((dims.width + tile_size.width - 1) / tile_size.width);
       int processed = 0;
       
       for (int y = 0; y < dims.height; y += tile_size.height) {
           for (int x = 0; x < dims.width; x += tile_size.width) {
               auto tile = stage_ptr->GetTile(x, y, tile_size.width, tile_size.height);
               
               // Call Python callback
               callback(processed, total_tiles);
               ++processed;
           }
       }
   }
   
   // Binding
   m.def("process_with_callback", &ProcessWithCallback,
         "Process image with progress callback");

Error Handling
~~~~~~~~~~~~~~

Convert C++ exceptions to Python exceptions:

.. code-block:: cpp

   PyStage SafeCreateSource(const std::string& filename) {
       try {
           return CreateTiffStage(filename);
       } catch (const std::runtime_error& e) {
           throw py::value_error(std::string("Failed to open TIFF: ") + e.what());
       } catch (const std::exception& e) {
           throw py::runtime_error(std::string("Unexpected error: ") + e.what());
       }
   }

Documentation Strings
~~~~~~~~~~~~~~~~~~~~~

Add comprehensive docstrings:

.. code-block:: cpp

   m.def("create_gradient", &CreateGradientStage,
         py::arg("width"),
         py::arg("height"),
         py::arg("channels") = 3,
         R"pbdoc(
           Create a procedural gradient image.
           
           The gradient goes from black (left) to white (right).
           
           Parameters
           ----------
           width : int
               Width of the gradient image in pixels
           height : int
               Height of the gradient image in pixels
           channels : int, optional
               Number of channels (default: 3)
               
           Returns
           -------
           Image
               Image instance with gradient source
               
           Examples
           --------
           >>> img = fim.Image.from_gradient(512, 512, 3)
           >>> img.write_png("gradient.png")
         )pbdoc");

Best Practices
--------------

1. **Reuse C++ operators**: Never duplicate operator logic in Python wrappers
2. **Use PyStageAdapter**: Bridge type-erased stages to CRTP operators
3. **Factory pattern**: Create PyStage instances via factory functions in `factory.h`
4. **Type erasure**: Use StageConcept interface for runtime polymorphism
5. **Error handling**: Convert C++ exceptions to Python exceptions
6. **Documentation**: Provide comprehensive docstrings
7. **Testing**: Test all Python bindings thoroughly
8. **NumPy integration**: Support zero-copy when possible
9. **Pythonic API**: Use Python naming conventions (snake_case)
10. **Memory management**: Let pybind11 and smart pointers handle ownership

**The Golden Rule:**

.. important::

   Every operator should be implemented **once** in C++ and **reused** in Python 
   via PyStageAdapter. Never create separate Python-specific operator implementations.

Common Pitfalls
---------------

**Duplicating operator logic:**

.. code-block:: cpp

   // ❌ BAD: Creating Python-specific operator class
   class PyCrop : public StageConcept {
   public:
       PyCrop(std::shared_ptr<StageConcept> input, int x, int y, int w, int h)
           : input_(std::move(input)), x_(x), y_(y), w_(w), h_(h) {}
       
       Tile GetTile(int x, int y, int width, int height) const override {
           // Duplicate crop logic here...
       }
   };
   
   // ✅ GOOD: Reusing C++ operator via adapter
   inline PyStage ApplyCrop(const PyStage& input, int x, int y, int w, int h) {
       PyStageAdapter adapter(input.GetStagePtr());
       auto crop = Crop<PyStageAdapter>(adapter, x, y, w, h);
       auto model = std::make_unique<StageModel<Crop<PyStageAdapter>>>(
           std::move(crop)
       );
       return PyStage(std::move(model));
   }

**Forgetting to validate PyStage:**

.. code-block:: cpp

   // Bad: No validation
   PyStage ApplyOperator(const PyStage& input) {
       auto stage_ptr = input.GetStagePtr();  // Might be null!
       // ...
   }
   
   // Good: GetStagePtr() validates automatically
   PyStage ApplyOperator(const PyStage& input) {
       auto stage_ptr = input.GetStagePtr();  // CheckValid() called
       // Safe to use
   }

**Incorrect lifetime management:**

.. code-block:: cpp

   // Bad: Returning reference to temporary
   const StageConcept& GetStage(const PyStage& input) {
       auto ptr = input.GetStagePtr();
       return *ptr;  // Dangling reference!
   }
   
   // Good: Return shared_ptr
   std::shared_ptr<StageConcept> GetStage(const PyStage& input) {
       return input.GetStagePtr();
   }

**Not handling GIL for callbacks:**

.. code-block:: cpp

   // Bad: Calling Python from C++ thread
   void ProcessInThread(py::function callback) {
       std::thread([callback]() {
           callback();  // GIL not held! Crash!
       }).join();
   }
   
   // Good: Acquire GIL before calling Python
   void ProcessInThread(py::function callback) {
       std::thread([callback]() {
           py::gil_scoped_acquire acquire;
           callback();  // Safe
       }).join();
   }

Next Steps
----------

- Reference existing bindings in `src/python/bindings.cpp`
- See pybind11 documentation: https://pybind11.readthedocs.io/
- Test your bindings thoroughly with pytest
- Add Python type hints in `python/fim/__init__.pyi`


fimage Architecture
===================

fimage is designed around the CRTP (Curiously Recurring Template Pattern) to achieve 
zero-overhead abstraction with compile-time polymorphism. This document describes the 
core architecture, design patterns, and implementation details.

.. raw:: html

   <div class="architecture-diagram">
       <h3>🏗️ System Architecture Overview</h3>
   </div>

Architecture Layers
-------------------

.. mermaid::

   graph TB
       subgraph python_api["Python API Layer"]
           PyImage[Python Image Class]
           PyStage[PyStage Handle]
           PyFactory[Factory Functions]
       end
       
       subgraph type_erasure["Type Erasure & Bridging Layer"]
           StageConcept[StageConcept Interface]
           StageModel[StageModel&lt;T&gt; Wrapper]
           PyStageAdapter[PyStageAdapter Bridge]
       end
       
       subgraph cpp_core["C++ CRTP Core"]
           ImageTemplate[Image&lt;SourceType&gt;]
           SourceBase[SourceBase&lt;Derived&gt;]
           OperatorBase[OperatorBase&lt;Derived, Input&gt;]
           SinkBase[SinkBase&lt;Derived&gt;]
       end
       
       subgraph implementations["Concrete Implementations (Shared by C++ & Python)"]
           TiffSource[TiffSource]
           PngSource[PngSource]
           MemorySource[MemorySource]
           Crop[Crop&lt;InputType&gt;]
           Downsample[Downsample&lt;InputType&gt;]
           Stack[Stack&lt;InputType&gt;]
           TiffSink[TiffSink]
           LodePngSink[LodePngSink]
           MemorySink[MemorySink]
       end
       
       subgraph utilities["Utilities"]
           ThreadPool[Thread Pool]
           ParallelMixin[ParallelizationMixin]
       end
       
       PyImage --> PyStage
       PyStage --> StageConcept
       PyFactory --> PyStageAdapter
       PyStageAdapter --> Crop
       PyStageAdapter --> Downsample
       PyStageAdapter --> Stack
       PyFactory --> StageModel
       StageModel --> ImageTemplate
       ImageTemplate --> SourceBase
       ImageTemplate --> OperatorBase
       SourceBase --> TiffSource
       SourceBase --> PngSource
       SourceBase --> MemorySource
       OperatorBase --> Crop
       OperatorBase --> Downsample
       OperatorBase --> Stack
       SinkBase --> TiffSink
       SinkBase --> LodePngSink
       SinkBase --> MemorySink
       SourceBase --> ParallelMixin
       OperatorBase --> ParallelMixin
       SinkBase --> ParallelMixin
       ParallelMixin --> ThreadPool

Core Concepts
-------------

CRTP Pattern
~~~~~~~~~~~~

The Curiously Recurring Template Pattern (CRTP) is the foundation of fimage's 
zero-overhead abstraction. Each pipeline component inherits from a base class 
template that takes the derived class as a template parameter.

**Benefits:**

- **Compile-time polymorphism**: No virtual function calls
- **Static dispatch**: All method calls resolved at compile time
- **Inlining**: Compiler can inline entire pipeline
- **Type safety**: Compile-time type checking

**Example:**

.. code-block:: cpp

   // Base class template
   template <typename Derived>
   class SourceBase {
   public:
       ImageInfo GetDimensions() const {
           return static_cast<const Derived*>(this)->GetDimensions();
       }
       
       Tile GetTile(int x, int y, int width, int height) const {
           return static_cast<const Derived*>(this)->GetTile(x, y, width, height);
       }
   };
   
   // Derived class
   class TiffSource : public SourceBase<TiffSource> {
   public:
       ImageInfo GetDimensions() const { /* implementation */ }
       Tile GetTile(int x, int y, int width, int height) const { /* implementation */ }
   };

Lazy Evaluation
~~~~~~~~~~~~~~~

Operations build a computation graph but don't execute until materialization. 
This happens when a sink processes the pipeline.

**Pipeline Construction (Lazy):**

.. code-block:: cpp

   // Build the pipeline - no computation yet
   auto pipeline = fim::Image<fim::TiffSource>("input.tiff")
       .Crop(100, 100, 512, 512)      // Returns Image<Crop<TiffSource>>
       .Downsample(2);                // Returns Image<Downsample<Crop<TiffSource>>>
   
   // Type of 'pipeline':
   // Image<Downsample<Crop<TiffSource>>>

**Materialization (Execution):**

.. code-block:: cpp

   // Render triggers execution
   pipeline.Render(fim::LodePngSink("output.png"));
   
   // Sink requests tiles from Downsample
   // Downsample requests tiles from Crop
   // Crop requests tiles from TiffSource
   // Data flows back up the chain

Tile-Based Processing
~~~~~~~~~~~~~~~~~~~~~

Images are processed in rectangular tiles, which are the fundamental unit of computation.

**Tile Flow:**

.. mermaid::

   sequenceDiagram
       participant Sink
       participant Downsample
       participant Crop
       participant TiffSource
       
       Sink->>Downsample: GetTile(0, 0, 256, 256)
       Downsample->>Crop: GetTile(0, 0, 512, 512)
       Crop->>TiffSource: GetTile(100, 100, 512, 512)
       TiffSource-->>Crop: Tile data
       Crop-->>Downsample: Cropped tile
       Downsample-->>Sink: Downsampled tile
       Sink->>Downsample: GetTile(256, 0, 256, 256)
       Note over Sink,TiffSource: Process continues tile by tile

Core Components
---------------

SourceBase<Derived>
~~~~~~~~~~~~~~~~~~~

Base class for all image sources. Sources provide image data from files or memory.

.. doxygenclass:: fim::SourceBase
   :members:
   :protected-members:

**Required Methods:**

- `GetDimensions()`: Return image width, height, and channels
- `GetTile(x, y, width, height)`: Return a rectangular tile of data
- `GetIdealTileSize()`: Suggest optimal tile size for this source

**Implementations:**

.. doxygenclass:: fim::TiffSource
   :members:

.. doxygenclass:: fim::PngSource
   :members:

.. doxygenclass:: fim::MemorySource
   :members:

OperatorBase<Derived, InputType>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Base class for all image operators. Operators transform data from an input stage.

.. doxygenclass:: fim::OperatorBase
   :members:
   :protected-members:

**Required Methods:**

- `GetDimensions()`: Return output image dimensions (may differ from input)
- `GetTile(x, y, width, height)`: Return transformed tile
- `GetIdealTileSize()`: Propagate or modify ideal tile size

**Ownership:**

Operators own their input by value (after architecture improvements). This prevents 
dangling references when chaining operations.

.. code-block:: cpp

   template <typename Derived, typename InputType>
   class OperatorBase {
   private:
       InputType input_;  // Owned by value, not reference
   };

**Implementations:**

.. doxygenclass:: fim::Crop
   :members:

.. doxygenclass:: fim::Downsample
   :members:

.. doxygenclass:: fim::Stack
   :members:

SinkBase<Derived>
~~~~~~~~~~~~~~~~~

Base class for all sinks. Sinks materialize the pipeline by requesting tiles 
and assembling the final output.

.. doxygenclass:: fim::SinkBase
   :members:
   :protected-members:

**Required Methods:**

- `Render(source)`: Process all tiles from source and produce output

**Move Semantics:**

Sinks support move semantics for efficient resource transfer:

.. code-block:: cpp

   template <typename SinkType>
   void Render(SinkType &&sink) {
       std::forward<SinkType>(sink).Render(source_);
   }

**Implementations:**

.. doxygenclass:: fim::TiffSink
   :members:

.. doxygenclass:: fim::LodePngSink
   :members:

.. doxygenclass:: fim::MemorySink
   :members:

ParallelizationMixin<Derived>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

CRTP mixin providing threading utilities to all pipeline components.

.. doxygenclass:: fim::ParallelizationMixin
   :members:
   :protected-members:

**Usage:**

.. code-block:: cpp

   class TiffSource : public SourceBase<TiffSource> {
       void ProcessTiles() {
           auto &pool = GetThreadPool();
           
           if (ShouldParallelize(num_tiles)) {
               // Parallel processing
               pool.parallelize_loop(0, num_tiles, [&](int i) {
                   ProcessTile(i);
               });
           } else {
               // Sequential processing
               for (int i = 0; i < num_tiles; ++i) {
                   ProcessTile(i);
               }
           }
       }
   };

Python Bindings Architecture
-----------------------------

Python-C++ Operator Reuse
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Architecture Principle:**

fimage employs a **reuse-based architecture** where Python operators delegate to 
the same C++ CRTP implementations used in the native API. This eliminates code 
duplication and ensures consistent behavior across both language bindings.

**The Challenge:**

- C++ operators are CRTP templates requiring compile-time types
- Python needs runtime polymorphism via type erasure
- We must bridge these two paradigms without duplicating logic

**The Solution: Four-Component Design:**

1. **StageConcept**: Abstract interface with virtual methods (type erasure)
2. **StageModel<T>**: Wraps CRTP types to implement StageConcept
3. **PyStageAdapter**: Bridges StageConcept back to CRTP interface
4. **PyStage**: Python-facing handle holding `shared_ptr<StageConcept>`

Type Erasure Pattern
~~~~~~~~~~~~~~~~~~~~

.. mermaid::

   classDiagram
       class StageConcept {
           <<interface>>
           +GetDimensions() ImageInfo
           +GetTile() Tile
           +GetIdealTileSize() TileSize
       }
       
       class StageModel~T~ {
           -T stage_
           +GetDimensions() ImageInfo
           +GetTile() Tile
           +GetIdealTileSize() TileSize
       }
       
       class PyStage {
           -shared_ptr~StageConcept~ stage_
           +GetStagePtr() shared_ptr
           +CheckValid() void
       }
       
       StageConcept <|.. StageModel
       PyStage o-- StageConcept

**StageConcept Interface:**

.. code-block:: cpp

   class StageConcept {
   public:
       virtual ~StageConcept() = default;
       virtual ImageInfo GetDimensions() const = 0;
       virtual Tile GetTile(int x, int y, int width, int height) const = 0;
       virtual TileSize GetIdealTileSize() const = 0;
   };

**StageModel<T> Wrapper:**

.. code-block:: cpp

   template <typename T>
   class StageModel : public StageConcept {
   public:
       explicit StageModel(T stage) : stage_(std::move(stage)) {}
       
       ImageInfo GetDimensions() const override {
           return stage_.GetDimensions();
       }
       
       Tile GetTile(int x, int y, int width, int height) const override {
           return stage_.GetTile(x, y, width, height);
       }
       
       TileSize GetIdealTileSize() const override {
           return stage_.GetIdealTileSize();
       }
   
   private:
       T stage_;  // CRTP type stored by value
   };

**PyStage Handle:**

.. code-block:: cpp

   class PyStage {
   public:
       template <typename T>
       explicit PyStage(T stage)
           : stage_(std::make_shared<StageModel<T>>(std::move(stage))) {}
       
       std::shared_ptr<StageConcept> GetStagePtr() const {
           CheckValid();  // Validate before returning
           return stage_;
       }
   
   private:
       std::shared_ptr<StageConcept> stage_;
   };

PyStageAdapter Bridge
~~~~~~~~~~~~~~~~~~~~~

**The Missing Link:**

To reuse C++ operators in Python, we need to bridge from type-erased `StageConcept` 
back to the CRTP interface expected by operators. This is where `PyStageAdapter` 
comes in.

**PyStageAdapter Implementation:**

.. code-block:: cpp

   // Provides CRTP interface by forwarding to type-erased stage
   class PyStageAdapter {
   public:
       explicit PyStageAdapter(std::shared_ptr<const StageConcept> stage)
           : stage_(std::move(stage)) {}
       
       // Copy/move constructors allow passing by value to operators
       PyStageAdapter(const PyStageAdapter&) = default;
       PyStageAdapter(PyStageAdapter&&) noexcept = default;
       
       // CRTP-compatible methods forward to virtual interface
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

**Reuse Flow:**

.. mermaid::

   sequenceDiagram
       participant Python as Python: stage.crop(...)
       participant Factory as ApplyCrop Factory
       participant Adapter as PyStageAdapter
       participant Operator as Crop&lt;PyStageAdapter&gt;
       participant Model as StageModel
       
       Python->>Factory: crop(x, y, w, h)
       Factory->>Adapter: PyStageAdapter(stage_ptr)
       Factory->>Operator: Crop&lt;PyStageAdapter&gt;(adapter, ...)
       Note over Operator: C++ operator logic executes
       Operator-->>Factory: Crop&lt;PyStageAdapter&gt; instance
       Factory->>Model: StageModel(crop_operator)
       Model-->>Factory: unique_ptr&lt;StageModel&gt;
       Factory-->>Python: PyStage(model)

Factory Functions
~~~~~~~~~~~~~~~~~

Factory functions orchestrate the reuse pattern:

.. code-block:: cpp

   // include/fim/python/factory.h
   
   // Create PyStage from TIFF file (source)
   inline PyStage MakeTiffSource(const std::string& filename) {
       auto source = TiffSource::Create(filename);
       auto model = std::make_unique<StageModel<TiffSource>>(std::move(source));
       return PyStage(std::move(model));
   }
   
   // Apply crop operator (reuses C++ Crop via adapter)
   inline PyStage ApplyCrop(const PyStage& input, int x, int y, int w, int h) {
       // 1. Wrap type-erased stage in adapter
       PyStageAdapter adapter(input.GetStagePtr());
       
       // 2. Instantiate C++ operator with adapter as input type
       auto crop = Crop<PyStageAdapter>(adapter, x, y, w, h);
       
       // 3. Type-erase operator back to StageConcept
       auto model = std::make_unique<StageModel<Crop<PyStageAdapter>>>(
           std::move(crop)
       );
       
       // 4. Return as PyStage
       return PyStage(std::move(model));
   }
   
   // Stack multiple inputs (each adapted separately)
   inline PyStage ApplyStack(const std::vector<PyStage>& inputs) {
       // Adapt each input
       std::vector<PyStageAdapter> adapters;
       adapters.reserve(inputs.size());
       for (const auto& input : inputs) {
           adapters.emplace_back(input.GetStagePtr());
       }
       
       // Use C++ Stack operator
       auto stack = Stack<PyStageAdapter>(std::move(adapters));
       auto model = std::make_unique<StageModel<Stack<PyStageAdapter>>>(
           std::move(stack)
       );
       return PyStage(std::move(model));
   }

**Key Insight:**

By using `PyStageAdapter`, we transform the type-erased `StageConcept` into a 
concrete type that satisfies the CRTP operator's `InputType` requirements. The 
adapter forwards all method calls to the virtual interface, introducing a small 
runtime cost only at stage boundaries—not within the operator's tight loops.

Memory Management
-----------------

Ownership Semantics
~~~~~~~~~~~~~~~~~~~

**Sources**: Own their resources (file handles, buffers)

.. code-block:: cpp

   class TiffSource {
   private:
       std::unique_ptr<TIFF, TiffDeleter> tiff_;  // RAII for file handle
   };

**Operators**: Own their inputs by value

.. code-block:: cpp

   template <typename Derived, typename InputType>
   class OperatorBase {
   public:
       explicit OperatorBase(InputType input)
           : input_(std::move(input)) {}  // Move input, take ownership
   
   private:
       InputType input_;  // Owned by value
   };

**Sinks**: Temporary or move-constructed

.. code-block:: cpp

   // Sink with pre-allocated buffer
   fim::MemorySink sink(width, height, channels);
   pipeline.Render(std::move(sink));  // Move sink for efficiency

Move Optimization
~~~~~~~~~~~~~~~~~

**Pipeline Construction:**

.. code-block:: cpp

   // Each method consumes 'this' (rvalue ref-qualifier)
   auto Crop(int x, int y, int width, int height) && {
       return Image<fim::Crop<SourceType>>(
           fim::Crop<SourceType>(std::move(source_), x, y, width, height)
       );
   }
   
   // Usage:
   fim::CreateTiffImage("input.tiff")  // Temporary Image<TiffSource>
       .Crop(0, 0, 512, 512)           // Moves source_ into Crop
       .Downsample(2);                 // Moves Crop into Downsample

**Benefits:**

- No copies of pipeline stages
- Efficient resource transfer
- Compiler optimizations (RVO, NRVO)

Thread Safety
-------------

**Read-only Operations**: Thread-safe (multiple threads can read simultaneously)

**Tile Processing**: Each tile processed independently

**Shared Resources**: Protected by internal synchronization

.. code-block:: cpp

   // Multiple threads can safely read from same source
   auto &pool = GetThreadPool();
   pool.parallelize_loop(0, num_tiles, [&](int i) {
       auto tile = source.GetTile(tile_x, tile_y, tile_w, tile_h);
       ProcessTile(tile);
   });

Performance Characteristics
----------------------------

**Compile-time Optimizations:**

- CRTP eliminates virtual function overhead
- Template instantiation allows inlining
- Dead code elimination for unused operations
- Constant folding and loop unrolling

**Runtime Optimizations:**

- Tile-based processing matches hardware cache sizes
- Parallel tile processing via thread pool
- Move semantics minimize copying
- Lazy evaluation avoids unnecessary computation

**Memory Profile:**

- O(tile_size) memory usage, not O(image_size)
- RAII ensures no memory leaks
- Smart pointers for shared resources
- Zero-copy NumPy conversion (Python)

Extension Points
----------------

Adding a new component requires implementing the appropriate CRTP interface:

**New Source:**

.. code-block:: cpp

   class MySource : public SourceBase<MySource> {
   public:
       ImageInfo GetDimensions() const { /* ... */ }
       Tile GetTile(int x, int y, int width, int height) const { /* ... */ }
       TileSize GetIdealTileSize() const { /* ... */ }
   };

**New Operator:**

.. code-block:: cpp

   template <typename InputType>
   class MyOperator : public OperatorBase<MyOperator<InputType>, InputType> {
   public:
       explicit MyOperator(InputType input) 
           : OperatorBase<MyOperator, InputType>(std::move(input)) {}
       
       ImageInfo GetDimensions() const { /* ... */ }
       Tile GetTile(int x, int y, int width, int height) const { /* ... */ }
       TileSize GetIdealTileSize() const { /* ... */ }
   };

**New Sink:**

.. code-block:: cpp

   class MySink : public SinkBase<MySink> {
   public:
       template <typename SourceType>
       void Render(const SourceType& source) { /* ... */ }
   };

See :doc:`guides/index` for detailed tutorials.


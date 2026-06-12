# FIM Library Test Suite

This directory contains comprehensive unit tests and integration tests for the FIM (Fast Image) library. The tests are built using Google Test (gtest) framework and provide extensive coverage of all library components.

## Test Structure

The test suite is organized to mirror the main library structure:

```
tests/
├── types_test.cpp              # Tests for core types (Tile, ImageDimensions, TileSize)
├── image_test.cpp              # Tests for high-level Image interface
├── integration_test.cpp        # Full pipeline integration tests
├── operators/
│   ├── crop_test.cpp          # Tests for crop operator
│   └── downsample_test.cpp    # Tests for downsample operator
├── sources/
│   ├── png_source_test.cpp    # Tests for PNG image source
│   └── tiff_source_test.cpp   # Tests for TIFF image source
├── sinks/
│   ├── png_sink_test.cpp      # Tests for PNG image sink
│   └── tiff_sink_test.cpp     # Tests for TIFF image sink
├── meson.build                # Build configuration for tests
└── README.md                  # This file
```

## Building and Running Tests

### Prerequisites

- Google Test (gtest) library installed
- Meson build system
- C++20 compatible compiler
- libtiff development libraries

### Build Configuration

Tests are built as an optional component. To enable tests:

```bash
# Configure build with tests enabled
meson setup builddir -Dtests=true

# Build the library and tests
meson compile -C builddir

# Run all tests
meson test -C builddir
```

### Running Individual Tests

```bash
# Run a specific test
meson test -C builddir types_test

# Run tests with verbose output
meson test -C builddir --verbose

# Run tests with specific pattern
meson test -C builddir --no-suite integration
```

## Test Coverage

### Core Types (`types_test.cpp`)

- **Tile class**: Construction, data management, padding functionality
- **ImageDimensions**: Basic dimension handling for different channel counts
- **TileSize**: Tile size specifications and validation

### Image Interface (`image_test.cpp`)

- **Image creation**: PNG and TIFF image creation functions
- **Method chaining**: Crop and downsample operations
- **Rendering**: Output to PNG and TIFF sinks
- **Error handling**: Invalid files and parameters

### Operators

#### Crop Operator (`operators/crop_test.cpp`)

- Basic cropping functionality
- Boundary handling (clipping, out-of-bounds)
- Coordinate translation
- Tile-based processing
- Different channel configurations

#### Downsample Operator (`operators/downsample_test.cpp`)

- Average pooling implementation
- Non-divisible dimensions handling
- Various downsample factors
- Tile size calculations
- Error handling for invalid factors

### Sources

#### PNG Source (`sources/png_source_test.cpp`)

- PNG file loading and decoding
- Lazy loading behavior
- Tile extraction
- Move semantics
- Error handling for invalid files

#### TIFF Source (`sources/tiff_source_test.cpp`)

- TIFF file loading (uses existing checkerboard.svs)
- Tile and strip format handling
- Bounds checking
- Move semantics
- Error handling

### Sinks

#### PNG Sink (`sinks/png_sink_test.cpp`)

- RGB, RGBA, and grayscale output
- Tile assembly
- Error handling for unsupported formats
- File size validation

#### TIFF Sink (`sinks/tiff_sink_test.cpp`)

- Tiled and strip TIFF output
- Custom tile sizes
- Various channel configurations
- Compression handling

### Integration Tests (`integration_test.cpp`)

- **Full pipeline tests**: Source → Operators → Sink
- **Chained operations**: Multiple operators in sequence
- **Real file processing**: Tests with actual image files
- **Error handling**: End-to-end error scenarios
- **Performance**: Large image processing (when test data available)

## Test Data

The test suite uses both generated and existing test data:

### Generated Test Data

Tests automatically generate small test images for:

- PNG files with various formats (RGB, RGBA, grayscale)
- Checkerboard and gradient patterns
- Different image sizes

### Existing Test Data

Some tests use existing project files:

- `checkerboard.svs`: Large TIFF file for comprehensive testing
- Tests gracefully skip when files are not available

## Mock Objects

The test suite includes mock implementations for testing:

- **MockSource**: Simulates image sources with predictable patterns
- Pattern generation for testing operators and sinks
- Configurable dimensions and channel counts

## Test Utilities

Common utilities across tests:

- **Test file cleanup**: Automatic cleanup of generated test files
- **Pattern generation**: Consistent test patterns for validation
- **Error injection**: Testing error handling paths

## Continuous Integration

Tests are designed to run in CI environments:

- No external dependencies beyond gtest and libtiff
- Automatic test data generation
- Graceful handling of missing optional test files
- Reasonable timeout values for all tests

## Test Guidelines

When adding new tests:

1. **Follow naming conventions**: `ComponentTest` for test fixtures
2. **Clean up resources**: Use SetUp/TearDown for file management
3. **Test error conditions**: Include negative test cases
4. **Use meaningful assertions**: Clear expected vs actual values
5. **Document complex tests**: Add comments for non-obvious test logic

## Performance Considerations

- Tests use small image sizes for speed
- Large image tests are optional (require test data)
- Memory-efficient mock objects
- Timeout values allow for debug builds

## Debugging Tests

For debugging failing tests:

```bash
# Run with debug output
meson test -C builddir --verbose --no-suite slow

# Run specific test with gdb
gdb builddir/tests/types_test

# Check test file generation
ls -la builddir/tests/
```

## Known Limitations

1. **TIFF tests**: Some tests require checkerboard.svs file
2. **Platform dependencies**: File path handling may vary
3. **Compiler variations**: C++20 feature support required
4. **Memory usage**: Large image tests may require significant RAM

## Contributing

When adding new functionality:

1. Add corresponding unit tests
2. Update integration tests if needed
3. Ensure all tests pass
4. Add documentation for new test patterns
5. Consider performance impact of new tests

## Test Results

Expected test results on successful run:

- All unit tests: PASS
- Integration tests: PASS (with test data) or SKIP (without)
- Total test time: < 60 seconds on modern hardware
- Memory usage: < 1GB peak

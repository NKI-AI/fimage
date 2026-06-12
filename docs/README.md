# fimage Documentation

This directory contains the documentation for the fimage library, including both Sphinx-based narrative documentation and Doxygen-generated C++ API reference.

## Building Documentation

### Using Bazel (Recommended)

The recommended way to build the documentation is using Bazel:

```bash
# Build all documentation (Sphinx + Doxygen)
bazelisk build //aifo/fimage/docs:all_docs

# View Sphinx HTML documentation
open bazel-bin/aifo/fimage/docs/sphinx_html/_build/html/index.html

# View Doxygen HTML documentation
open bazel-bin/aifo/fimage/docs/html/index.html
```

### Using Make (Local Development)

For local development without Bazel:

```bash
cd aifo/fimage/docs

# Install dependencies
make install-deps

# Build all documentation
make html

# Clean build artifacts
make clean
```

### Using Sphinx Directly

For rapid iteration on Sphinx documentation:

```bash
cd aifo/fimage/docs

# Install dependencies
pip install -r requirements_darwin.txt  # macOS
# or
pip install -r requirements_linux.txt   # Linux

# Build Doxygen first (Sphinx needs the XML)
doxygen Doxyfile

# Build Sphinx documentation
sphinx-build -b html source build/sphinx/html

# View the results
open build/sphinx/html/index.html
```

## Documentation Structure

```
docs/
├── BUILD.bazel              # Bazel build configuration
├── Doxyfile                 # Standalone Doxygen configuration
├── Makefile                 # Convenience build targets
├── requirements.in          # Python dependencies (input)
├── requirements_darwin.txt  # Locked dependencies for macOS
├── requirements_linux.txt   # Locked dependencies for Linux
└── source/                  # Sphinx source files
    ├── conf.py              # Sphinx configuration
    ├── index.rst            # Documentation home page
    ├── overview.rst         # Library overview
    ├── architecture.rst     # Architecture deep dive
    ├── api/                 # API reference
    │   ├── index.rst
    │   ├── cpp_api.rst      # C++ API documentation
    │   └── python_api.rst   # Python API documentation
    ├── guides/              # Extension guides
    │   ├── index.rst
    │   ├── adding_source.rst
    │   ├── adding_operator.rst
    │   ├── adding_sink.rst
    │   └── python_bindings.rst
    ├── examples/            # Examples
    │   └── index.rst
    └── _static/             # Static assets
        └── custom.css       # Custom styling

```

## Documentation Components

### Sphinx Documentation

- **Overview**: High-level introduction to fimage
- **Architecture**: Detailed explanation of CRTP, lazy evaluation, and type erasure
- **API Reference**: Combined C++ and Python API documentation
- **Extension Guides**: Step-by-step tutorials for extending the library
- **Examples**: Complete working examples

### Doxygen Documentation

- Complete C++ API reference
- Interactive class diagrams
- Call graphs and dependency graphs
- Source code browser

## Updating Documentation

### Adding New Content

1. Create/edit `.rst` files in `source/`
2. Add to appropriate `toctree` in index files
3. Rebuild documentation to verify

### Updating Dependencies

```bash
# Edit requirements.in
vim requirements.in

# Regenerate lock files
bazelisk run //aifo/fimage/docs:generate_requirements_lock

# Or with Make
make lock
```

### Adding Doxygen Comments

Use Doxygen-style comments in C++ code:

```cpp
/**
 * @brief Brief description of the function.
 *
 * Detailed description with more information about
 * what the function does and how to use it.
 *
 * @param param1 Description of first parameter
 * @param param2 Description of second parameter
 * @return Description of return value
 * @throw std::exception Description of exception condition
 */
ReturnType FunctionName(Type1 param1, Type2 param2);
```

## Viewing Documentation Locally

After building:

**Sphinx HTML**: The main narrative documentation with integrated C++ API

- Bazel: `bazel-bin/aifo/fimage/docs/sphinx_html/_build/html/index.html`
- Make: `build/sphinx/html/index.html`

**Doxygen HTML**: Detailed C++ API with interactive diagrams

- Bazel: `bazel-bin/aifo/fimage/docs/html/index.html`
- Make: `build/doxygen/html/index.html`

## Troubleshooting

### Missing Dependencies

```bash
# Reinstall dependencies
make install-deps
```

### Doxygen Warnings

Doxygen warnings are suppressed by default (`QUIET = YES`). To see them:

```bash
# Edit Doxyfile
QUIET = NO

# Rebuild
make doxygen
```

### Sphinx Build Warnings

Sphinx warnings are shown but don't fail the build (`--keep-going`). To see all warnings:

```bash
# Remove --keep-going from BUILD.bazel extra_opts
# Or build with strict mode:
sphinx-build -W -b html source build/sphinx/html
```

## CI/CD

The documentation is built and tested in CI:

- Bazel build verification
- Link checking
- Documentation coverage (future)

## Contributing

When contributing to documentation:

1. Follow the existing structure and style
2. Use consistent terminology
3. Provide code examples where appropriate
4. Test documentation builds locally
5. Run spell check on new content

## License

Apache 2.0 License - see LICENSE file in the repository root.

# Makefile for fimage documentation
# This provides convenience targets for building docs locally without Bazel

# Detect OS for platform-specific requirements
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    REQUIREMENTS = requirements_darwin.txt
    PLATFORM = darwin
else
    REQUIREMENTS = requirements_linux.txt
    PLATFORM = linux
endif

# Directories
BUILDDIR = build
SPHINXBUILD = sphinx-build
SPHINXSOURCE = source
DOXYGENBUILDDIR = $(BUILDDIR)/doxygen
SPHINXBUILDDIR = $(BUILDDIR)/sphinx

# Colors for output
BLUE = \033[0;34m
GREEN = \033[0;32m
NC = \033[0m # No Color

.PHONY: help clean docs html doxygen sphinx install-deps

help:
	@echo "$(BLUE)fimage Documentation Build System$(NC)"
	@echo ""
	@echo "Available targets:"
	@echo "  $(GREEN)help$(NC)         - Show this help message"
	@echo "  $(GREEN)install-deps$(NC) - Install Python dependencies"
	@echo "  $(GREEN)doxygen$(NC)      - Build Doxygen C++ API documentation"
	@echo "  $(GREEN)sphinx$(NC)       - Build Sphinx documentation"
	@echo "  $(GREEN)html$(NC)         - Build both Doxygen and Sphinx (default)"
	@echo "  $(GREEN)clean$(NC)        - Remove build artifacts"
	@echo ""
	@echo "Platform detected: $(PLATFORM)"
	@echo ""
	@echo "$(BLUE)Note:$(NC) For production builds, use Bazel:"
	@echo "  bazelisk build //aifo/fimage/docs:all_docs"

# Default target
html: doxygen sphinx
	@echo "$(GREEN)✓ Documentation built successfully!$(NC)"
	@echo ""
	@echo "View documentation:"
	@echo "  Sphinx: file://$(PWD)/$(SPHINXBUILDDIR)/html/index.html"
	@echo "  Doxygen: file://$(PWD)/$(DOXYGENBUILDDIR)/html/index.html"

# Install Python dependencies
install-deps:
	@echo "$(BLUE)Installing documentation dependencies...$(NC)"
	pip install -r $(REQUIREMENTS)
	@echo "$(GREEN)✓ Dependencies installed$(NC)"

# Build Doxygen documentation
doxygen:
	@echo "$(BLUE)Building Doxygen documentation...$(NC)"
	@mkdir -p $(DOXYGENBUILDDIR)
	doxygen Doxyfile
	@echo "$(GREEN)✓ Doxygen documentation built$(NC)"

# Build Sphinx documentation
sphinx: doxygen
	@echo "$(BLUE)Building Sphinx documentation...$(NC)"
	@mkdir -p $(SPHINXBUILDDIR)
	$(SPHINXBUILD) -b html $(SPHINXSOURCE) $(SPHINXBUILDDIR)/html
	@echo "$(GREEN)✓ Sphinx documentation built$(NC)"

# Clean build artifacts
clean:
	@echo "$(BLUE)Cleaning build artifacts...$(NC)"
	rm -rf $(BUILDDIR)
	@echo "$(GREEN)✓ Build artifacts removed$(NC)"

# Generate requirements lock files (requires uv)
lock:
	@echo "$(BLUE)Generating requirements lock files...$(NC)"
	@if command -v uv >/dev/null 2>&1; then \
		uv pip compile requirements.in -o requirements_linux.txt --python-platform x86_64-unknown-linux-gnu; \
		uv pip compile requirements.in -o requirements_darwin.txt --python-platform aarch64-apple-darwin; \
		echo "$(GREEN)✓ Lock files generated$(NC)"; \
	else \
		echo "$(BLUE)Note: uv not found. Install with: pip install uv$(NC)"; \
		echo "Or use Bazel: bazelisk run //aifo/fimage/docs:generate_requirements_lock"; \
	fi

# Alias targets
docs: html
all: html


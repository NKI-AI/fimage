project('fimage', 'c', 'cpp',
  version : '0.1.2',
  license : 'Apache-2.0',
  meson_version : '>=1.3',
  default_options : [
    'warning_level=1',
    'cpp_std=c++20',
    'default_library=static',
    # Optimized by default (mirrors the Bazel `--config=release` -O3 -DNDEBUG).
    # Override on the CLI with e.g. `meson setup builddir --buildtype=debug`.
    'buildtype=release',
    'b_ndebug=if-release',
  ])

fs = import('fs')
cpp = meson.get_compiler('cpp')

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
# NKI-AI libraries (git wraps standalone). fastslide exposes the `fastslide`
# dependency via meson.override_dependency in its own meson.build.
aifocore_dep = dependency('aifocore')
fastslide_dep = dependency('fastslide')

# Codecs + SIMD. All have wrap fallbacks; system copies are used when present.
hwy_dep = dependency('libhwy', fallback : ['highway', 'hwy_dep'])
tiff_dep = dependency('libtiff-4')
spng_dep = dependency('spng')
lodepng_dep = dependency('lodepng')
lz4_dep = dependency('liblz4')
zstd_dep = dependency('libzstd')

fimage_deps = [
  aifocore_dep, fastslide_dep, hwy_dep, tiff_dep, spng_dep, lodepng_dep,
  lz4_dep, zstd_dep,
]

# ---------------------------------------------------------------------------
# Library sources
# ---------------------------------------------------------------------------
inc = include_directories('include')

fimage_sources = [
  'src/utilities/compressor.cpp',
  'src/utilities/decompressor.cpp',
  'src/operators/resize_simd.cpp',
  'src/sources/black_source.cpp',
  'src/sources/memory_source.cpp',
  'src/sources/tiff_source.cpp',
  'src/sources/lodepng_png_source.cpp',
  'src/sources/fastslide_source.cpp',
  'src/sources/associated_image_source.cpp',
  'src/sources/fimage_source.cpp',
  'src/sinks/memory_sink.cpp',
  'src/sinks/tiff_sink.cpp',
  'src/sinks/lodepng_png_sink.cpp',
  'src/sinks/spng_sink.cpp',
  'src/sinks/fimage_sink.cpp',
]

# `-Wno-unused-parameter` is GCC/Clang-only; drop it on MSVC (cl.exe rejects it).
fimage_cpp_args = cpp.get_supported_arguments(['-Wno-unused-parameter'])

# When building the Python wheel the static library and public headers are NOT
# installed: meson-python would otherwise dump them into the wheel. Only the
# nanobind extension + Python package belong in the wheel.
install_cpp = not get_option('build_python')

fimage_lib = library('fimage',
  fimage_sources,
  include_directories : inc,
  dependencies : fimage_deps,
  cpp_args : fimage_cpp_args,
  install : install_cpp)

if install_cpp
  install_subdir('include/fim', install_dir : get_option('includedir'))
endif

fim_dep = declare_dependency(
  include_directories : inc,
  link_with : fimage_lib,
  dependencies : fimage_deps)

meson.override_dependency('fimage', fim_dep)

# ---------------------------------------------------------------------------
# Python extension (nanobind)
# ---------------------------------------------------------------------------
# Built only for wheels (meson-python sets -Dbuild_python=true). nanobind and
# robin-map are resolved from subprojects/*.wrap; with --wrap-mode=forcefallback
# they compile from source, so the resulting `_fim` extension is self-contained.
if get_option('build_python')
  py = import('python').find_installation(pure : false)
  nanobind_dep = dependency('nanobind')

  py.extension_module('_fim',
    [
      'src/python/bindings.cpp',
      'src/python/associated_images_dict.cpp',
      'src/python/deferred_black_canvas.cpp',
    ],
    include_directories : inc,
    dependencies : [nanobind_dep, fim_dep],
    cpp_args : fimage_cpp_args,
    install : true,
    subdir : 'fim')

  # Pure-Python package + checked-in nanobind stub (matches the Bazel wheel);
  # both install by basename into the `fim` package dir.
  py.install_sources(
    [
      'python/fim/__init__.py',
      'python/fim/_fim.pyi',
    ],
    subdir : 'fim')
endif

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
# gtest unit tests are colocated with the implementation under src/ (mirroring
# the Bazel cc_test targets). gtest is resolved from subprojects/gtest.wrap.
if get_option('build_tests') and not get_option('build_python')
  gtest_dep = dependency('gtest', main : false, required : true)
  gtest_main_dep = dependency('gtest_main', required : true)

  fimage_test_sources = [
    'src/image_test.cpp',
    'src/integration_test.cpp',
    'src/types_test.cpp',
    'src/parallel_performance_test.cpp',
    'src/sources/memory_source_test.cpp',
    'src/sources/black_source_test.cpp',
    'src/sources/png_source_test.cpp',
    'src/sources/tiff_source_test.cpp',
    'src/sources/fimage_source_test.cpp',
    'src/sources/fastslide_source_test.cpp',
    'src/sinks/fimage_sink_test.cpp',
    'src/sinks/memory_sink_test.cpp',
    'src/sinks/lodepng_sink_test.cpp',
    'src/sinks/tiff_sink_test.cpp',
    'src/sinks/spng_sink_test.cpp',
    'src/sinks/move_semantics_test.cpp',
    'src/operators/crop_test.cpp',
    'src/operators/paste_test.cpp',
    'src/operators/downsample_test.cpp',
    'src/operators/ownership_test.cpp',
    'src/operators/zero_tile_test.cpp',
    'src/operators/resize_test.cpp',
    'src/utilities/buffer_pool_test.cpp',
    'src/utilities/compressor_test.cpp',
    'src/utilities/decompressor_test.cpp',
  ]

  foreach test_src : fimage_test_sources
    test_name = test_src.replace('src/', '').replace('.cpp', '').replace('/', '_')
    test_exe = executable(test_name,
      test_src,
      include_directories : inc,
      dependencies : [fim_dep, gtest_dep, gtest_main_dep],
      cpp_args : fimage_cpp_args,
      install : false)
    test(test_name, test_exe, timeout : 300)
  endforeach
endif

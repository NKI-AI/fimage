// Copyright 2025 Jonas Teuwen. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/**
 * @file bindings.cpp
 * @brief Python bindings for the fim image processing library.
 * @author Jonas Teuwen
 * @date 2025
 *
 * This file defines the Python module for fim, exposing the lazy image
 * processing pipeline with PyVips-style API through nanobind.
 */

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "fastslide/core/slide_descriptor.h"
#include "fim/operators/resize/kernels.h"
#include "fim/operators/resize/resample.h"
#include "fim/python/associated_images_dict.h"
#include "fim/python/deferred_black_canvas.h"
#include "fim/python/factory.h"
#include "fim/python/py_stage.h"
#include "fim/sinks/fimage_sink.h"
#include "fim/sinks/lodepng_png_sink.h"
#include "fim/sinks/memory_sink.h"
#include "fim/sinks/spng_sink.h"
#include "fim/sinks/tiff_sink.h"
#include "fim/sources/fimage_source.h"
#include "fim/types.h"
#include "fim/utilities/debug_flags.h"
#include "fim/utilities/layout_utils.h"

namespace nb = nanobind;
namespace fs = std::filesystem;

namespace fim {
namespace python {

namespace {

// Translate the variant-valued image properties into a Python dict.
nb::dict ToPythonPropertiesDict(const ImageProperties& props) {
  nb::dict out;
  for (const auto& [key, value] : props) {
    std::visit([&](const auto& v) { out[nb::cast(key)] = nb::cast(v); }, value);
  }
  return out;
}

// Helper: build a NumPy array that owns ``vec``'s buffer through a capsule
// without copying. The vector is moved to the heap; the capsule deletes it
// when Python no longer references the array.
template <typename T>
nb::ndarray<nb::numpy, T> VectorToNumpy(std::vector<T>&& vec,
                                        const std::vector<std::size_t>& shape) {
  auto* heap = new std::vector<T>(std::move(vec));
  nb::capsule owner(
      heap, [](void* p) noexcept { delete static_cast<std::vector<T>*>(p); });
  return nb::ndarray<nb::numpy, T>(heap->data(), shape.size(), shape.data(),
                                   owner);
}

}  // namespace

/**
 * @brief Converts a PyStage to a NumPy array with appropriate dtype.
 *
 * The pipeline is rendered into a memory sink and the result is wrapped as a
 * zero-copy NumPy array via a capsule that owns the buffer.
 */
nb::object ToNumpy(const PyStage& stage,
                   std::optional<DataLayout> output_layout = std::nullopt) {
  auto dims = stage.GetDimensions();
  const DataLayout requested_layout = output_layout.value_or(dims.layout);

  MemorySink sink = MemorySink::Create();

  const bool debug = debug_flags::IsEnabled("FIM_DEBUG_TO_NUMPY");
  if (debug) {
    std::fprintf(stderr,
                 "[fim] ToNumpy: dims=%dx%dx%d pixel_type=%d src_layout=%d "
                 "output_layout=%d\n",
                 dims.GetWidth(), dims.GetHeight(), dims.channels,
                 static_cast<int>(dims.pixel_type),
                 static_cast<int>(dims.layout),
                 static_cast<int>(requested_layout));
    std::fflush(stderr);
  }

  {
    nb::gil_scoped_release release;
    sink.Render(stage.GetStageConcept());
  }

  const DataLayout src_layout = dims.layout;
  const std::size_t height = static_cast<std::size_t>(dims.GetHeight());
  const std::size_t width = static_cast<std::size_t>(dims.GetWidth());
  const std::size_t channels = static_cast<std::size_t>(dims.channels);

  const std::vector<std::size_t> shape =
      requested_layout == DataLayout::kChannelsLast
          ? std::vector<std::size_t>{height, width, channels}
          : std::vector<std::size_t>{channels, height, width};

  switch (dims.pixel_type) {
    case PixelType::kUInt8: {
      std::vector<uint8_t> data = sink.TakeDataAs<uint8_t>();
      if (src_layout != requested_layout) {
        data = layout_utils::ConvertBufferDataLayout<uint8_t>(
            data, dims.GetWidth(), dims.GetHeight(), dims.channels, src_layout,
            requested_layout);
      }
      return nb::cast(VectorToNumpy<uint8_t>(std::move(data), shape));
    }
    case PixelType::kUInt16: {
      std::vector<uint16_t> data = sink.TakeDataAs<uint16_t>();
      if (src_layout != requested_layout) {
        data = layout_utils::ConvertBufferDataLayout<uint16_t>(
            data, dims.GetWidth(), dims.GetHeight(), dims.channels, src_layout,
            requested_layout);
      }
      return nb::cast(VectorToNumpy<uint16_t>(std::move(data), shape));
    }
    case PixelType::kFloat32: {
      std::vector<float> data = sink.TakeDataAs<float>();
      if (src_layout != requested_layout) {
        data = layout_utils::ConvertBufferDataLayout<float>(
            data, dims.GetWidth(), dims.GetHeight(), dims.channels, src_layout,
            requested_layout);
      }
      return nb::cast(VectorToNumpy<float>(std::move(data), shape));
    }
    default:
      throw std::runtime_error("Unsupported pixel type for numpy conversion");
  }
}

void WritePng(const PyStage& stage, const fs::path& filename) {
  LodePngSink sink = LodePngSink::Create(filename.string());
  nb::gil_scoped_release release;
  sink.Render(stage.GetStageConcept());
}

void WriteSpng(const PyStage& stage, const fs::path& filename) {
  SpngSink sink = SpngSink::Create(filename.string());
  nb::gil_scoped_release release;
  sink.Render(stage.GetStageConcept());
}

void WriteTiff(const PyStage& stage, const fs::path& filename) {
  TiffSink sink = TiffSink::Create(filename.string());
  nb::gil_scoped_release release;
  sink.Render(stage.GetStageConcept());
}

void WriteFimage(const PyStage& stage, const fs::path& filename,
                 int compression = 0, int tile_width = 0, int tile_height = 0,
                 double mpp_x = 0.0, double mpp_y = 0.0) {
  CompressionType comp_type = static_cast<CompressionType>(compression);
  TileSize tile_size(tile_width, tile_height);

  FImageSink sink =
      FImageSink::Create(filename.string(), tile_size, comp_type, mpp_x, mpp_y);
  nb::gil_scoped_release release;
  sink.Render(stage.GetStageConcept());
}

PyStage Crop(const PyStage& stage, std::tuple<int, int> position,
             std::tuple<int, int> size) {
  const auto [x, y] = position;
  const auto [width, height] = size;
  return ApplyCrop(stage, x, y, width, height);
}

PyStage Downsample(const PyStage& stage, int factor) {
  return ApplyDownsample(stage, factor);
}

PyStage Resize(
    const PyStage& stage, int target_width, int target_height,
    fim::resize::KernelType kernel = fim::resize::KernelType::kLanczos3,
    std::optional<fim::resize::Box> box = std::nullopt) {
  return ApplyResize(stage, target_width, target_height, kernel, box);
}

std::tuple<int, int, int> GetDimensions(const PyStage& stage) {
  auto dims = stage.GetDimensions();
  return {dims.GetWidth(), dims.GetHeight(), dims.channels};
}

nb::object GetTile(const PyStage& stage, int x, int y, int width, int height,
                   DataLayout output_layout = DataLayout::kChannelsLast) {
  Tile tile = stage.GetTile(x, y, width, height);
  if (tile.layout != output_layout) {
    tile = layout_utils::ConvertTileDataLayout(tile, output_layout);
  }

  PixelType pixel_type = tile.GetPixelType();

  const std::size_t tw = static_cast<std::size_t>(tile.width);
  const std::size_t th = static_cast<std::size_t>(tile.height);
  const std::size_t tc = static_cast<std::size_t>(tile.channels);

  const std::vector<std::size_t> shape =
      output_layout == DataLayout::kChannelsLast
          ? std::vector<std::size_t>{th, tw, tc}
          : std::vector<std::size_t>{tc, th, tw};

  switch (pixel_type) {
    case PixelType::kUInt8: {
      std::vector<uint8_t> data = std::move(tile.GetDataAsMut<uint8_t>());
      return nb::cast(VectorToNumpy<uint8_t>(std::move(data), shape));
    }
    case PixelType::kUInt16: {
      std::vector<uint16_t> data = std::move(tile.GetDataAsMut<uint16_t>());
      return nb::cast(VectorToNumpy<uint16_t>(std::move(data), shape));
    }
    case PixelType::kFloat32: {
      std::vector<float> data = std::move(tile.GetDataAsMut<float>());
      return nb::cast(VectorToNumpy<float>(std::move(data), shape));
    }
    default:
      throw std::runtime_error("Unsupported pixel type for numpy conversion");
  }
}

namespace {

// Returns true iff ``array``'s memory layout is C-contiguous. We perform the
// check ourselves to forbid silently copying non-contiguous arrays.
template <typename Array>
bool IsCContiguous(const Array& array) {
  if (array.ndim() == 0) {
    return true;
  }
  std::int64_t expected = 1;
  for (std::size_t i = array.ndim(); i-- > 0;) {
    if (array.stride(i) != expected) {
      return false;
    }
    expected *= static_cast<std::int64_t>(array.shape(i));
  }
  return true;
}

// Resolve numpy shape into (height, width, channels) given the layout.
struct HwcShape {
  int width = 0;
  int height = 0;
  int channels = 1;
};

template <typename Array>
HwcShape ResolveHwcShape(const Array& array, DataLayout layout) {
  HwcShape out;
  if (array.ndim() == 2) {
    out.height = static_cast<int>(array.shape(0));
    out.width = static_cast<int>(array.shape(1));
    out.channels = 1;
  } else if (layout == DataLayout::kChannelsLast) {
    out.height = static_cast<int>(array.shape(0));
    out.width = static_cast<int>(array.shape(1));
    out.channels = static_cast<int>(array.shape(2));
  } else {
    out.channels = static_cast<int>(array.shape(0));
    out.height = static_cast<int>(array.shape(1));
    out.width = static_cast<int>(array.shape(2));
  }
  return out;
}

template <typename T>
PyStage CopyArrayIntoStage(const nb::ndarray<const T, nb::device::cpu>& array,
                           const HwcShape& shape, DataLayout data_layout,
                           PixelType pixel_type) {
  const std::size_t total = static_cast<std::size_t>(shape.width) *
                            static_cast<std::size_t>(shape.height) *
                            static_cast<std::size_t>(shape.channels);
  const T* src = array.data();
  std::vector<T> data(src, src + total);
  if constexpr (std::is_same_v<T, uint8_t>) {
    return MakeMemorySource(std::move(data), shape.width, shape.height,
                            shape.channels, pixel_type, data_layout);
  } else {
    (void)pixel_type;
    return MakeMemorySourceTyped(std::move(data), shape.width, shape.height,
                                 shape.channels, data_layout);
  }
}

}  // namespace

PyStage FromNumpy(nb::ndarray<nb::device::cpu> array,
                  DataLayout data_layout = DataLayout::kChannelsLast) {
  if (array.ndim() != 2 && array.ndim() != 3) {
    throw std::invalid_argument(
        "Array must be 2-dimensional (height, width) for grayscale or "
        "3-dimensional for multi-channel images");
  }

  if (!IsCContiguous(array)) {
    throw std::invalid_argument(
        "from_numpy(): input array must be C-contiguous. "
        "Call numpy.ascontiguousarray(array) first.");
  }

  const HwcShape shape = ResolveHwcShape(array, data_layout);
  const auto dtype = array.dtype();

  if (dtype == nb::dtype<uint8_t>()) {
    auto typed =
        nb::cast<nb::ndarray<const uint8_t, nb::device::cpu>>(nb::cast(array));
    return CopyArrayIntoStage<uint8_t>(typed, shape, data_layout,
                                       PixelType::kUInt8);
  }
  if (dtype == nb::dtype<uint16_t>()) {
    auto typed =
        nb::cast<nb::ndarray<const uint16_t, nb::device::cpu>>(nb::cast(array));
    return CopyArrayIntoStage<uint16_t>(typed, shape, data_layout,
                                        PixelType::kUInt16);
  }
  if (dtype == nb::dtype<float>()) {
    auto typed =
        nb::cast<nb::ndarray<const float, nb::device::cpu>>(nb::cast(array));
    return CopyArrayIntoStage<float>(typed, shape, data_layout,
                                     PixelType::kFloat32);
  }

  throw std::invalid_argument(
      "Unsupported numpy dtype. Supported types: uint8, uint16, float32");
}

PyStage Stack(const std::vector<PyStage>& images, const std::string& axis) {
  return ApplyStack(images, axis);
}

PyStage Paste(const PyStage& background, const PyStage& foreground, int x,
              int y) {
  return ApplyPaste(background, foreground, x, y);
}

}  // namespace python
}  // namespace fim

NB_MODULE(_fim, m) {
  m.doc() = "FIM: Fast Image processing library with lazy evaluation";

  using namespace fim::python;

  // ---- Enumerations ---------------------------------------------------------
  nb::enum_<fim::resize::KernelType>(m, "KernelType")
      .value("NEAREST", fim::resize::KernelType::kNearest,
             "Nearest neighbor (no interpolation)")
      .value("LANCZOS2", fim::resize::KernelType::kLanczos2,
             "Lanczos kernel with radius 2")
      .value("LANCZOS3", fim::resize::KernelType::kLanczos3,
             "Lanczos kernel with radius 3")
      .value("MAGIC2021", fim::resize::KernelType::kMagic2021,
             "Magic Kernel Sharp 2021 with radius 4.5")
      .export_values();

  nb::enum_<fim::PixelType>(m, "PixelType", nb::is_arithmetic())
      .value("UINT8", fim::PixelType::kUInt8, "8-bit unsigned integer")
      .value("UINT16", fim::PixelType::kUInt16, "16-bit unsigned integer")
      .value("FLOAT32", fim::PixelType::kFloat32, "32-bit floating point")
      .export_values();

  nb::enum_<fim::DataLayout>(m, "DataLayout")
      .value("CHANNELS_LAST", fim::DataLayout::kChannelsLast,
             "Interleaved layout: RGBRGBRGB... (HWC format)")
      .value("CHANNELS_FIRST", fim::DataLayout::kChannelsFirst,
             "Planar layout: RRR...GGG...BBB... (CHW format)")
      .export_values();

  nb::enum_<fim::CompressionType>(m, "CompressionType", nb::is_arithmetic())
      .value("NONE", fim::CompressionType::kNone, "No compression")
      .value("LZ4", fim::CompressionType::kLZ4, "LZ4 compression (fast)")
      .value("ZSTD", fim::CompressionType::kZstd,
             "Zstandard compression (better ratio)")
      .export_values();

  // ---- Box ------------------------------------------------------------------
  nb::class_<fim::resize::Box>(m, "Box")
      .def(nb::init<>(), "Create an empty box")
      .def(nb::init<float, float, float, float>(), nb::arg("x1"), nb::arg("y1"),
           nb::arg("x2"), nb::arg("y2"),
           "Create a box with specified coordinates (x1, y1, x2, y2)")
      .def_rw("x1", &fim::resize::Box::x1, "Left edge in pixel coordinates")
      .def_rw("y1", &fim::resize::Box::y1, "Top edge in pixel coordinates")
      .def_rw("x2", &fim::resize::Box::x2, "Right edge in pixel coordinates")
      .def_rw("y2", &fim::resize::Box::y2, "Bottom edge in pixel coordinates")
      .def("width", &fim::resize::Box::Width, "Get width of the box")
      .def("height", &fim::resize::Box::Height, "Get height of the box")
      .def("__repr__", [](const fim::resize::Box& box) {
        return "Box(x1=" + std::to_string(box.x1) +
               ", y1=" + std::to_string(box.y1) +
               ", x2=" + std::to_string(box.x2) +
               ", y2=" + std::to_string(box.y2) + ")";
      });

  // ---- Associated images dicts ----------------------------------------------
  nb::class_<FastSlideAssociatedImagesDict>(m, "FastSlideAssociatedImagesDict")
      .def("__getitem__", &FastSlideAssociatedImagesDict::GetItem,
           nb::arg("name"), "Get associated image by name as lazy fim.Image")
      .def("__contains__", &FastSlideAssociatedImagesDict::Contains,
           nb::arg("name"), "Check if associated image exists")
      .def("keys", &FastSlideAssociatedImagesDict::Keys,
           "Get list of available associated image names")
      .def("__len__", &FastSlideAssociatedImagesDict::Len,
           "Get number of associated images")
      .def("__repr__", [](const FastSlideAssociatedImagesDict& dict) {
        const auto keys = dict.Keys();
        std::string keys_str;
        for (std::size_t i = 0; i < keys.size(); ++i) {
          if (i > 0)
            keys_str += ", ";
          keys_str += "'" + keys[i] + "'";
        }
        return "<AssociatedImages with " + std::to_string(dict.Len()) +
               " images: " + keys_str + ">";
      });

#if defined(FIM_WITH_OPENSLIDE)
  nb::class_<OpenSlideAssociatedImagesDict>(m, "OpenSlideAssociatedImagesDict")
      .def("__getitem__", &OpenSlideAssociatedImagesDict::GetItem,
           nb::arg("name"), "Get associated image by name as lazy fim.Image")
      .def("__contains__", &OpenSlideAssociatedImagesDict::Contains,
           nb::arg("name"), "Check if associated image exists")
      .def("keys", &OpenSlideAssociatedImagesDict::Keys,
           "Get list of available associated image names")
      .def("__len__", &OpenSlideAssociatedImagesDict::Len,
           "Get number of associated images")
      .def("__repr__", [](const OpenSlideAssociatedImagesDict& dict) {
        const auto keys = dict.Keys();
        std::string keys_str;
        for (std::size_t i = 0; i < keys.size(); ++i) {
          if (i > 0)
            keys_str += ", ";
          keys_str += "'" + keys[i] + "'";
        }
        return "<AssociatedImages with " + std::to_string(dict.Len()) +
               " images: " + keys_str + ">";
      });
#endif  // defined(FIM_WITH_OPENSLIDE)

  // ---- FastSlide context ----------------------------------------------------
  nb::class_<FastSlideContext>(m, "FastSlideContext")
      .def("at_level", &FastSlideContext::AtLevel, nb::arg("level"),
           "Get a specific pyramid level as an Image")
      .def("close", &FastSlideContext::Close,
           "Close the slide and release resources. Safe to call multiple "
           "times.")
      .def("__enter__",
           [](FastSlideContext& self) -> FastSlideContext& { return self; })
      .def(
          "__exit__",
          [](FastSlideContext& self, nb::object, nb::object, nb::object) {
            self.Close();
          },
          nb::arg("exc_type").none(), nb::arg("exc_value").none(),
          nb::arg("traceback").none())
      .def_prop_ro("level_count", &FastSlideContext::GetLevelCount,
                   "Number of pyramid levels")
      .def_prop_ro("mpp", &FastSlideContext::GetMpp,
                   "Microns per pixel as (mpp_x, mpp_y)")
      .def_prop_ro("format_name", &FastSlideContext::GetFormatName,
                   "Slide format name (e.g., 'SVS', 'MRXS')")
      .def_prop_ro("num_channels", &FastSlideContext::GetNumChannels,
                   "Number of color channels (constant across all levels)")
      .def_prop_ro("memory_format", &FastSlideContext::GetMemoryFormat,
                   "Memory layout format (constant across all levels)")
      .def_prop_ro(
          "level_dimensions",
          [](const FastSlideContext& ctx) {
            nb::tuple result =
                nb::steal<nb::tuple>(PyTuple_New(ctx.GetLevelCount()));
            for (int i = 0; i < ctx.GetLevelCount(); ++i) {
              auto dims = ctx.GetLevelDimensions(i);
              PyTuple_SET_ITEM(
                  result.ptr(), i,
                  nb::cast(std::make_tuple(dims.width, dims.height))
                      .release()
                      .ptr());
            }
            return result;
          },
          "Tuple of (width, height) tuples for each pyramid level")
      .def_prop_ro(
          "level_downsamples",
          [](const FastSlideContext& ctx) {
            nb::tuple result =
                nb::steal<nb::tuple>(PyTuple_New(ctx.GetLevelCount()));
            for (int i = 0; i < ctx.GetLevelCount(); ++i) {
              PyTuple_SET_ITEM(
                  result.ptr(), i,
                  nb::cast(ctx.GetLevelDownsample(i)).release().ptr());
            }
            return result;
          },
          "Tuple of downsample factors for each pyramid level")
      .def_prop_ro(
          "bounds",
          [](const FastSlideContext& ctx) {
            auto bounds = ctx.GetBounds();
            return std::make_tuple(std::make_tuple(bounds[0], bounds[1]),
                                   std::make_tuple(bounds[2], bounds[3]));
          },
          "Slide bounds as ((x, y), (width, height))")
      .def_prop_ro(
          "channel_metadata",
          [](const FastSlideContext& ctx) {
            const auto channels = ctx.GetChannelMetadata();
            nb::list result;
            for (const auto& ch : channels) {
              nb::dict ch_dict;
              ch_dict["name"] = ch.name;
              ch_dict["biomarker"] = ch.biomarker;
              ch_dict["color"] =
                  std::make_tuple(ch.color.r, ch.color.g, ch.color.b);
              ch_dict["exposure_time"] = ch.exposure_time;
              ch_dict["signal_units"] = ch.signal_units;
              nb::dict additional;
              for (const auto& [key, value] : ch.additional) {
                additional[nb::cast(key)] = nb::cast(value);
              }
              ch_dict["additional"] = additional;
              result.append(ch_dict);
            }
            return result;
          },
          "List of channel metadata dicts (empty for standard RGB slides)")
      .def_prop_ro(
          "properties",
          [](const FastSlideContext& ctx) {
            nb::dict out;
            for (const auto& [key, value] : ctx.GetAllProperties()) {
              out[nb::cast(key)] = nb::cast(value);
            }
            return out;
          },
          "Dictionary of all slide properties")
      .def_prop_ro(
          "associated_images",
          [](const FastSlideContext& ctx) {
            return FastSlideAssociatedImagesDict(ctx.GetSource());
          },
          "Lazy dict-like accessor for associated images (each value is a "
          "lazy fim.Image)")
      .def("__repr__", [](const FastSlideContext& ctx) {
        return "FastSlideContext(levels=" +
               std::to_string(ctx.GetLevelCount()) + ", format='" +
               ctx.GetFormatName() + "')";
      });

  m.def(
      "open_fastslide",
      [](const fs::path& filename) {
        return fim::python::OpenFastSlide(filename);
      },
      nb::arg("filename"),
      "Open a whole slide image and return a context manager that can "
      "provide multiple pyramid levels. Use with 'with' statement:\n\n"
      "    with fim.open_fastslide('slide.svs') as slide:\n"
      "        img = slide.at_level(0).crop(...).resize(...)");

#if defined(FIM_WITH_OPENSLIDE)
  // ---- OpenSlide context ----------------------------------------------------
  nb::class_<OpenSlideContext>(m, "OpenSlideContext")
      .def("at_level", &OpenSlideContext::AtLevel, nb::arg("level"),
           "Get a specific pyramid level as an Image")
      .def("close", &OpenSlideContext::Close,
           "Close the slide and release resources. Safe to call multiple "
           "times.")
      .def("__enter__",
           [](OpenSlideContext& self) -> OpenSlideContext& { return self; })
      .def(
          "__exit__",
          [](OpenSlideContext& self, nb::object, nb::object, nb::object) {
            self.Close();
          },
          nb::arg("exc_type").none(), nb::arg("exc_value").none(),
          nb::arg("traceback").none())
      .def_prop_ro("level_count", &OpenSlideContext::GetLevelCount,
                   "Number of pyramid levels")
      .def_prop_ro("mpp", &OpenSlideContext::GetMpp,
                   "Microns per pixel as (mpp_x, mpp_y)")
      .def_prop_ro("format_name", &OpenSlideContext::GetFormatName,
                   "Slide format name (e.g., 'aperio', 'hamamatsu')")
      .def_prop_ro("num_channels", &OpenSlideContext::GetNumChannels,
                   "Number of color channels (always 3 for OpenSlide RGB)")
      .def_prop_ro("memory_format", &OpenSlideContext::GetMemoryFormat,
                   "Memory layout format (constant across all levels)")
      .def_prop_ro(
          "level_dimensions",
          [](const OpenSlideContext& ctx) {
            nb::tuple result =
                nb::steal<nb::tuple>(PyTuple_New(ctx.GetLevelCount()));
            for (int i = 0; i < ctx.GetLevelCount(); ++i) {
              auto dims = ctx.GetLevelDimensions(i);
              PyTuple_SET_ITEM(
                  result.ptr(), i,
                  nb::cast(std::make_tuple(dims.width, dims.height))
                      .release()
                      .ptr());
            }
            return result;
          },
          "Tuple of (width, height) tuples for each pyramid level")
      .def_prop_ro(
          "level_downsamples",
          [](const OpenSlideContext& ctx) {
            nb::tuple result =
                nb::steal<nb::tuple>(PyTuple_New(ctx.GetLevelCount()));
            for (int i = 0; i < ctx.GetLevelCount(); ++i) {
              PyTuple_SET_ITEM(
                  result.ptr(), i,
                  nb::cast(ctx.GetLevelDownsample(i)).release().ptr());
            }
            return result;
          },
          "Tuple of downsample factors for each pyramid level")
      .def_prop_ro(
          "bounds",
          [](const OpenSlideContext& ctx) {
            auto bounds = ctx.GetBounds();
            return std::make_tuple(std::make_tuple(bounds[0], bounds[1]),
                                   std::make_tuple(bounds[2], bounds[3]));
          },
          "Slide bounds as ((x, y), (width, height))")
      .def_prop_ro(
          "properties",
          [](const OpenSlideContext& ctx) {
            nb::dict out;
            for (const auto& [key, value] : ctx.GetAllProperties()) {
              out[nb::cast(key)] = nb::cast(value);
            }
            return out;
          },
          "Dictionary of all OpenSlide properties")
      .def_prop_ro(
          "associated_images",
          [](const OpenSlideContext& ctx) {
            return OpenSlideAssociatedImagesDict(ctx.GetSource());
          },
          "Lazy dict-like accessor for associated images (each value is a "
          "lazy fim.Image)")
      .def("__repr__", [](const OpenSlideContext& ctx) {
        return "OpenSlideContext(levels=" +
               std::to_string(ctx.GetLevelCount()) + ", format='" +
               ctx.GetFormatName() + "')";
      });

  m.def(
      "open_openslide",
      [](const fs::path& filename, bool rgb) {
        return fim::python::OpenOpenSlide(filename, rgb);
      },
      nb::arg("filename"), nb::arg("rgb") = true,
      "Open a whole slide image using OpenSlide and return a context manager "
      "that can provide multiple pyramid levels. Use with 'with' statement:\n\n"
      "    with fim.open_openslide('slide.svs', rgb=True) as slide:\n"
      "        img = slide.at_level(0).crop(...).resize(...)\n\n"
      "Parameters:\n"
      "  filename: Path to the whole slide image file\n"
      "  rgb: If True (default), output RGB (3 channels); if False, output "
      "RGBA (4 channels)");
#endif  // defined(FIM_WITH_OPENSLIDE)

  // ---- Libtiff context ------------------------------------------------------
  nb::class_<LibtiffContext>(m, "LibtiffContext")
      .def("at_level", &LibtiffContext::AtLevel, nb::arg("level"),
           "Get a specific TIFF directory (page) as an Image")
      .def("close", &LibtiffContext::Close,
           "Close the TIFF context and release resources. Safe to call "
           "multiple times.")
      .def("__enter__",
           [](LibtiffContext& self) -> LibtiffContext& { return self; })
      .def(
          "__exit__",
          [](LibtiffContext& self, nb::object, nb::object, nb::object) {
            self.Close();
          },
          nb::arg("exc_type").none(), nb::arg("exc_value").none(),
          nb::arg("traceback").none())
      .def_prop_ro("level_count", &LibtiffContext::GetLevelCount,
                   "Number of TIFF directories (pages)")
      .def_prop_ro(
          "level_dimensions",
          [](const LibtiffContext& ctx) {
            nb::tuple result =
                nb::steal<nb::tuple>(PyTuple_New(ctx.GetLevelCount()));
            for (int i = 0; i < ctx.GetLevelCount(); ++i) {
              auto dims = ctx.GetLevelDimensions(i);
              PyTuple_SET_ITEM(
                  result.ptr(), i,
                  nb::cast(std::make_tuple(dims.width, dims.height))
                      .release()
                      .ptr());
            }
            return result;
          },
          "Tuple of (width, height) tuples for each TIFF directory (page)")
      .def_prop_ro(
          "properties",
          [](const LibtiffContext& ctx) {
            return ToPythonPropertiesDict(ctx.GetAllProperties());
          },
          "Dictionary of TIFF properties (includes num_pages, x_res, y_res)")
      .def("__repr__", [](const LibtiffContext& ctx) {
        return "LibtiffContext(pages=" + std::to_string(ctx.GetLevelCount()) +
               ")";
      });

  m.def(
      "open_libtiff",
      [](const fs::path& filename) {
        return fim::python::OpenLibtiff(filename);
      },
      nb::arg("filename"),
      "Open a multi-page TIFF using libtiff and return a context manager that "
      "can provide per-page access. Use with 'with' statement:\n\n"
      "    with fim.open_libtiff('image.tiff') as tiff:\n"
      "        print(tiff.level_count)\n"
      "        img0 = tiff.at_level(0)");

  // ---- Image (PyStage) ------------------------------------------------------
  nb::class_<PyStage>(m, "Image")
      .def(nb::init<>(), "Create an empty image (invalid)")

      .def_static(
          "from_libtiff",
          [](const fs::path& filename, int page) {
            return MakeTiffSource(filename, page);
          },
          nb::arg("filename"), nb::arg("page") = 0,
          "Create an image from a TIFF file using libtiff at the specified "
          "directory (page). Pages are 0-based.")
      .def_static(
          "open_libtiff",
          [](const fs::path& filename) {
            return fim::python::OpenLibtiff(filename);
          },
          nb::arg("filename"),
          "Open a multi-page TIFF using libtiff and return a context manager "
          "that can provide per-page access (see fim.open_libtiff)")
      .def_static(
          "from_png",
          [](const fs::path& filename) {
            return MakePngSource(filename.string());
          },
          nb::arg("filename"), "Create an image from a PNG file")
      .def_static(
          "from_fimage",
          [](const fs::path& filename) {
            return MakeFimageSource(filename.string());
          },
          nb::arg("filename"),
          "Create an image from an FImage file (custom format with tiling and "
          "compression support)")
      .def_static(
          "from_fastslide",
          [](const fs::path& filename, int level) {
            return MakeFastSlideSource(filename, level);
          },
          nb::arg("filename"), nb::arg("level") = 0,
          "Create an image from a whole slide image (SVS, MRXS, QPTIFF, etc.) "
          "at the specified pyramid level (0 = highest resolution)")
#if defined(FIM_WITH_OPENSLIDE)
      .def_static(
          "from_openslide",
          [](const fs::path& filename, int level, bool rgb) {
            return MakeOpenSlideSource(filename, level, rgb);
          },
          nb::arg("filename"), nb::arg("level") = 0, nb::arg("rgb") = true,
          "Create an image from a whole slide image using OpenSlide "
          "(SVS, MRXS, NDPI, etc.) at the specified pyramid level "
          "(0 = highest resolution). Set rgb=False to get RGBA output.")
#endif  // defined(FIM_WITH_OPENSLIDE)
      .def_static(
          "from_memory",
          [](const std::vector<uint8_t>& data, int width, int height,
             int channels) {
            std::vector<uint8_t> data_copy = data;
            return MakeMemorySource(std::move(data_copy), width, height,
                                    channels);
          },
          nb::arg("data"), nb::arg("width"), nb::arg("height"),
          nb::arg("channels"), "Create an image from raw memory data")
      .def_static(
          "from_numpy", &FromNumpy, nb::arg("array"),
          nb::arg("data_layout") = fim::DataLayout::kChannelsLast,
          "Create an image from a NumPy array. Supports:\n"
          "  - 2D arrays (height, width) for grayscale/single-channel images\n"
          "  - 3D arrays for multi-channel images:\n"
          "    - DataLayout.CHANNELS_LAST: (height, width, channels)\n"
          "    - DataLayout.CHANNELS_FIRST: (channels, height, width)\n")
      .def_static("stack", &Stack, nb::arg("images"), nb::arg("axis") = "bands",
                  "Stack multiple images along the specified axis")
      .def_static(
          "black",
          [](int width, int height) {
            return DeferredBlackCanvas(width, height);
          },
          nb::arg("width"), nb::arg("height"),
          "Create a black canvas with deferred channel determination. "
          "Channels are inferred from the first pasted image.")

      .def("crop", &Crop, nb::arg("position"), nb::arg("size"),
           "Crop the image to a rectangular region.\n"
           "Usage: crop((x, y), (width, height))")
      .def("downsample", &Downsample, nb::arg("factor"),
           "Downsample the image by the specified factor")
      .def("resize", &Resize, nb::arg("width"), nb::arg("height"),
           nb::arg("kernel") = fim::resize::KernelType::kLanczos3,
           nb::arg("box").none() = nb::none(),
           "Resize the image to the specified dimensions using high-quality "
           "resampling")
      .def(
          "paste",
          [](const PyStage& background, const PyStage& foreground, int x,
             int y) { return Paste(background, foreground, x, y); },
          nb::arg("image"), nb::arg("x"), nb::arg("y"),
          "Paste another image onto this image at the specified position. "
          "Returns a new image with the paste operation applied (lazy). "
          "The pasted image replaces pixels in the overlapping region.")
      .def(
          "to_layout",
          [](const PyStage& stage, fim::DataLayout layout) {
            return fim::python::ApplyConvertLayout(stage, layout);
          },
          nb::arg("data_layout"),
          "Convert the image's memory layout lazily (returns a new Image).")

      .def("write_png", &WritePng, nb::arg("filename"),
           "Write the image to a PNG file using lodepng")
      .def("write_spng", &WriteSpng, nb::arg("filename"),
           "Write the image to a PNG file using libspng (streaming, "
           "memory-efficient)")
      .def("write_tiff", &WriteTiff, nb::arg("filename"),
           "Write the image to a TIFF file")
      .def("write_fimage", &WriteFimage, nb::arg("filename"),
           nb::arg("compression") = 0, nb::arg("tile_width") = 0,
           nb::arg("tile_height") = 0, nb::arg("mpp_x") = 0.0,
           nb::arg("mpp_y") = 0.0,
           "Write the image to an FImage file. Parameters:\n"
           "  compression: 0=None, 1=LZ4 (fast), 2=Zstd (better compression)\n"
           "  tile_width, tile_height: Tile dimensions (0 for contiguous "
           "mode)\n"
           "  mpp_x, mpp_y: Microns per pixel (0.0 = unknown)")
      .def("to_numpy", &ToNumpy, nb::arg("data_layout").none() = nb::none(),
           "Convert the image to a NumPy array.\n\n"
           "By default (data_layout=None), this preserves the image's native "
           "layout.\n"
           "Pass DataLayout.CHANNELS_LAST for HWC or DataLayout.CHANNELS_FIRST "
           "for CHW.")
      .def(
          "__array__",
          [](const PyStage& stage, nb::object /*dtype*/, nb::object /*copy*/) {
            return ToNumpy(stage);
          },
          nb::arg("dtype").none() = nb::none(),
          nb::arg("copy").none() = nb::none(),
          "NumPy array protocol - allows np.asarray(fim_image) and "
          "PIL.Image.fromarray() to work")

      .def_prop_ro(
          "dimensions",
          [](const PyStage& stage) {
            auto dims = stage.GetDimensions();
            return std::make_tuple(dims.GetWidth(), dims.GetHeight(),
                                   dims.channels);
          },
          "Image dimensions as (width, height, channels)")
      .def("get_tile", &GetTile, nb::arg("x"), nb::arg("y"), nb::arg("width"),
           nb::arg("height"),
           nb::arg("data_layout") = fim::DataLayout::kChannelsLast,
           "Get a tile from the image as a NumPy array. Default is "
           "CHANNELS_LAST (HWC).")
      .def("is_valid", &PyStage::IsValid, "Check if the image is valid")
      .def_prop_ro(
          "dtype",
          [](const PyStage& stage) -> nb::object {
            if (!stage.IsValid()) {
              return nb::none();
            }
            auto dims = stage.GetDimensions();
            switch (dims.pixel_type) {
              case fim::PixelType::kUInt8:
                return nb::cast("uint8");
              case fim::PixelType::kUInt16:
                return nb::cast("uint16");
              case fim::PixelType::kFloat32:
                return nb::cast("float32");
              default:
                return nb::cast("unknown");
            }
          },
          "NumPy-style dtype string (e.g., 'uint8', 'uint16', 'float32')")
      .def_prop_ro(
          "properties",
          [](const PyStage& stage) {
            nb::dict props;
            if (!stage.IsValid()) {
              return props;
            }
            auto p = stage.GetPropertiesPtr();
            if (!p) {
              return props;
            }
            return ToPythonPropertiesDict(*p);
          },
          "Optional source properties (TIFF: num_pages, x_res, y_res)")
      .def_prop_ro(
          "pixel_type",
          [](const PyStage& stage) -> nb::object {
            if (!stage.IsValid()) {
              return nb::none();
            }
            auto dims = stage.GetDimensions();
            return nb::cast(dims.pixel_type);
          },
          "Pixel type as PixelType enum")

      .def("__repr__", [](const PyStage& stage) {
        if (!stage.IsValid()) {
          return std::string("Image(invalid)");
        }
        auto dims = stage.GetDimensions();

        std::string dtype_str;
        switch (dims.pixel_type) {
          case fim::PixelType::kUInt8:
            dtype_str = "uint8";
            break;
          case fim::PixelType::kUInt16:
            dtype_str = "uint16";
            break;
          case fim::PixelType::kFloat32:
            dtype_str = "float32";
            break;
          default:
            dtype_str = "unknown";
        }

        std::string layout_str;
        switch (dims.layout) {
          case fim::DataLayout::kChannelsLast:
            layout_str = "channels_last";
            break;
          case fim::DataLayout::kChannelsFirst:
            layout_str = "channels_first";
            break;
          default:
            layout_str = "unknown";
        }

        return "Image(width=" + std::to_string(dims.GetWidth()) +
               ", height=" + std::to_string(dims.GetHeight()) +
               ", channels=" + std::to_string(dims.channels) +
               ", dtype=" + dtype_str + ", layout=" + layout_str + ")";
      });

  // ---- DeferredBlackCanvas --------------------------------------------------
  nb::class_<DeferredBlackCanvas>(m, "DeferredBlackCanvas")
      .def(nb::init<int, int>(), nb::arg("width"), nb::arg("height"),
           "Create a deferred black canvas with specified dimensions. "
           "Channel count is inferred from the first pasted image.")
      .def(
          "paste",
          [](const DeferredBlackCanvas& canvas, const PyStage& image, int x,
             int y) { return canvas.Paste(image, x, y); },
          nb::arg("image"), nb::arg("x"), nb::arg("y"),
          "Paste an image onto the black canvas at the specified position. "
          "This creates the actual black source with channels inferred from "
          "the pasted image and returns a PyStage that can be further "
          "processed.")
      .def("get_width", &DeferredBlackCanvas::GetWidth,
           "Get canvas width in pixels")
      .def("get_height", &DeferredBlackCanvas::GetHeight,
           "Get canvas height in pixels")
      .def("__repr__", [](const DeferredBlackCanvas& canvas) {
        return "DeferredBlackCanvas(width=" +
               std::to_string(canvas.GetWidth()) +
               ", height=" + std::to_string(canvas.GetHeight()) + ")";
      });

  // ---- Exception translation -----------------------------------------------
  // Map ``std::invalid_argument`` to ``ValueError`` (consistent with the
  // pre-nanobind behaviour). All other ``std::exception`` derivatives are
  // translated by nanobind into ``RuntimeError`` automatically.
  nb::register_exception_translator(
      [](const std::exception_ptr& p, void* /*payload*/) {
        try {
          std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
          PyErr_SetString(PyExc_ValueError, e.what());
        }
      });

  m.attr("__version__") = "0.1.2";
}

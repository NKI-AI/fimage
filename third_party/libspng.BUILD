load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "libspng",
    srcs = [
        "spng/spng.c",
    ],
    hdrs = [
        "spng/spng.h",
    ],
    defines = [
        "SPNG_STATIC",  # Ensure consumers don't import SPNG symbols as a DLL.
    ],
    copts = [
        "-std=c99",
    ],
    includes = ["spng"],
    deps = [
        "@zlib",
    ],
)

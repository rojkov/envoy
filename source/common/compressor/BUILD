licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "zlib_compressor_lib",
    srcs = ["zlib_compressor_impl.cc"],
    hdrs = ["zlib_compressor_impl.h"],
    external_deps = ["zlib"],
    deps = [
        "//include/envoy/compressor:compressor_interface",
        "//source/common/buffer:buffer_lib",
        "//source/common/common:assert_lib",
        "//source/common/common:stack_array",
    ],
)

envoy_cc_library(
    name = "brotli_compressor_lib",
    srcs = ["brotli_compressor_impl.cc"],
    hdrs = ["brotli_compressor_impl.h"],
    external_deps = ["brotlienc"],
    deps = [
        "//include/envoy/compressor:compressor_interface",
        "//source/common/common:stack_array",
    ],
)

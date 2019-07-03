licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "decompressor_lib",
    srcs = ["decompressor.cc"],
    hdrs = ["decompressor.h"],
    deps = [
        "//include/envoy/decompressor:decompressor_interface",
        "//include/envoy/http:filter_interface",
        "//include/envoy/runtime:runtime_interface",
        "//include/envoy/stats:stats_macros",
        "//source/common/buffer:buffer_lib",
        "//source/common/http:header_map_lib",
        "//source/common/protobuf",
        "@envoy_api//envoy/config/filter/http/decompressor/v2:decompressor_proto_cc",
    ],
)

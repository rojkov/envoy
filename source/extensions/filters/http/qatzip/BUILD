licenses(["notice"])  # Apache 2

# HTTP L7 filter that performs qatzip compression

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "qatzip_filter_lib",
    srcs = ["qatzip_filter.cc"],
    hdrs = ["qatzip_filter.h"],
    deps = [
        "//source/common/compressor:qatzip_compressor_lib",
        "//source/extensions/filters/http/common/compressor:compressor_lib",
        "@envoy_api//envoy/config/filter/http/qatzip/v2:qatzip_cc",
    ],
)

envoy_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        "//source/extensions/filters/http:well_known_names",
        "//source/extensions/filters/http/common:factory_base_lib",
        "//source/extensions/filters/http/qatzip:qatzip_filter_lib",
    ],
)

licenses(["notice"])  # Apache 2

# HTTP L7 filter that performs gzip compression
# Public docs: docs/root/configuration/http_filters/gzip_filter.rst

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "gunzip_filter_lib",
    srcs = ["gunzip_filter.cc"],
    hdrs = ["gunzip_filter.h"],
    deps = [
        "//source/common/decompressor:zlib_decompressor_lib",
        "//source/extensions/filters/http/common/decompressor:decompressor_lib",
        "@envoy_api//envoy/config/filter/http/gunzip/v2:gunzip_cc",
    ],
)

envoy_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":gunzip_filter_lib",
        "//source/extensions/filters/http:well_known_names",
        "//source/extensions/filters/http/common:factory_base_lib",
    ],
)

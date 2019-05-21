licenses(["notice"])  # Apache 2

# HTTP L7 filter that performs brotli compression
# Public docs: docs/root/configuration/http_filters/brotli_filter.rst

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "brotli_filter_lib",
    srcs = ["brotli_filter.cc"],
    hdrs = ["brotli_filter.h"],
    deps = [
        "//source/common/compressor:brotli_compressor_lib",
        "//source/extensions/filters/http/common/compressor:compressor_lib",
        "@envoy_api//envoy/extensions/filters/http/brotli/v3:pkg_cc_proto",
    ],
)

envoy_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        "//source/extensions/filters/http:well_known_names",
        "//source/extensions/filters/http/brotli:brotli_filter_lib",
        "//source/extensions/filters/http/common:factory_base_lib",
    ],
)

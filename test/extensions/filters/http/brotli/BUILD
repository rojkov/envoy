licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load(
    "//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

envoy_package()

envoy_extension_cc_test(
    name = "brotli_filter_test",
    srcs = ["brotli_filter_test.cc"],
    extension_name = "envoy.filters.http.brotli",
    deps = [
        "//source/common/compressor:brotli_compressor_lib",
        "//source/common/decompressor:brotli_decompressor_lib",
        "//source/common/protobuf:utility_lib",
        "//source/extensions/filters/http/brotli:brotli_filter_lib",
        "//test/mocks/http:http_mocks",
        "//test/mocks/runtime:runtime_mocks",
        "//test/test_common:utility_lib",
    ],
)

envoy_extension_cc_test(
    name = "brotli_filter_integration_test",
    srcs = [
        "brotli_filter_integration_test.cc",
    ],
    extension_name = "envoy.filters.http.brotli",
    deps = [
        "//source/common/decompressor:brotli_decompressor_lib",
        "//source/extensions/filters/http/brotli:config",
        "//test/integration:http_integration_lib",
        "//test/test_common:simulated_time_system_lib",
        "//test/test_common:utility_lib",
    ],
)

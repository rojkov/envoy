licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "decompressor_test",
    srcs = ["zlib_decompressor_impl_test.cc"],
    deps = [
        "//source/common/common:assert_lib",
        "//source/common/common:hex_lib",
        "//source/common/compressor:zlib_compressor_lib",
        "//source/common/decompressor:zlib_decompressor_lib",
        "//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "brotli_decompressor_test",
    srcs = ["brotli_decompressor_impl_test.cc"],
    deps = [
        "//source/common/common:assert_lib",
        "//source/common/compressor:brotli_compressor_lib",
        "//source/common/decompressor:brotli_decompressor_lib",
        "//test/test_common:utility_lib",
    ],
)

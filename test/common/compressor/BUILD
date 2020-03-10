licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_fuzz_test",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_fuzz_test(
    name = "compressor_fuzz_test",
    srcs = ["compressor_fuzz_test.cc"],
    corpus = "compressor_corpus",
    deps = [
        "//source/common/buffer:buffer_lib",
        "//source/common/common:assert_lib",
        "//source/common/compressor:zlib_compressor_lib",
        "//source/common/decompressor:zlib_decompressor_lib",
    ],
)

envoy_cc_test(
    name = "zlib_compressor_test",
    srcs = ["zlib_compressor_impl_test.cc"],
    deps = [
        "//source/common/common:assert_lib",
        "//source/common/common:hex_lib",
        "//source/common/compressor:zlib_compressor_lib",
        "//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "brotli_compressor_test",
    srcs = ["brotli_compressor_impl_test.cc"],
    deps = [
        "//source/common/common:assert_lib",
        "//source/common/common:hex_lib",
        "//source/common/compressor:brotli_compressor_lib",
        "//source/common/decompressor:brotli_decompressor_lib",
        "//test/test_common:utility_lib",
    ],
)

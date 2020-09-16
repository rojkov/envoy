load(
    "//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load(
    "//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_extension_cc_test(
    name = "qatzip_decompressor_impl_test",
    srcs = ["qatzip_decompressor_impl_test.cc"],
    extension_name = "envoy.compression.qatzip.decompressor",
    deps = [
        "//source/common/stats:isolated_store_lib",
        "//source/extensions/compression/qatzip/compressor:compressor_lib",
        "//source/extensions/compression/qatzip/decompressor:config",
        "//test/mocks/server:factory_context_mocks",
        "//test/test_common:utility_lib",
    ],
)

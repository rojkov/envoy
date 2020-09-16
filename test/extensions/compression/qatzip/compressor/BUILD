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
    name = "compressor_test",
    srcs = ["qatzip_compressor_impl_test.cc"],
    extension_name = "envoy.compression.qatzip.compressor",
    deps = [
        "//source/extensions/compression/qatzip/compressor:config",
        "//source/extensions/compression/qatzip/decompressor:decompressor_lib",
        "//test/mocks/server:factory_context_mocks",
        "//test/test_common:utility_lib",
    ],
)

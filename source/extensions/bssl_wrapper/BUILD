licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "bssl_wrapper_lib",
    srcs = [
        "bssl_wrapper.cc",
    ],
    hdrs = [
        "bssl_wrapper.h",
    ],
    deps = [
        "@openssl//:ssl",
    ],
)

envoy_cc_library(
    name = "crypto_bytestring_lib",
    srcs = [
        "bytestring.cc",
    ],
    hdrs = [
        "bytestring.h",
    ],
    deps = [
        ":bssl_wrapper_lib",
        "@openssl//:ssl",
    ],
)

envoy_cc_library(
    name = "openssl_impl_lib",
    srcs = [
        "openssl_impl.cc",
    ],
    hdrs = [
        "openssl_impl.h",
    ],
    deps = [
        ":bssl_wrapper_lib",
        "@openssl//:ssl",
    ],
)

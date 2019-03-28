licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "bssl_compat_cbs_lib",
    srcs = [
        "cbs.cc",
    ],
    hdrs = [
        "cbs.h",
    ],
    deps = select({
        "//bazel:openssl": ["@openssl//:ssl"],
        "//conditions:default": [],
    }),
)

envoy_cc_library(
    name = "bssl_compat_lib",
    srcs = [
        "bssl.cc",
    ],
    hdrs = [
        "bssl.h",
    ],
    deps = select({
        "//bazel:openssl": ["@openssl//:ssl"],
        "//conditions:default": [],
    }),
)

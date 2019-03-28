licenses(["notice"])  # Apache 2

# TLS inspector filter for examining various TLS parameters before routing to a FilterChain.
# Public docs: docs/root/configuration/listener_filters/tls_inspector.rst

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "tls_inspector_lib",
    hdrs = ["tls_inspector.h"],
    deps = select({
        "//bazel:openssl": ["//source/extensions/filters/listener/tls_inspector/openssl:tls_inspector_lib"],
        "//conditions:default": ["//source/extensions/filters/listener/tls_inspector/boringssl:tls_inspector_lib"],
    }),
)

envoy_cc_library(
    name = "config",
    deps = select({
        "//bazel:openssl": ["//source/extensions/filters/listener/tls_inspector/openssl:config"],
        "//conditions:default": ["//source/extensions/filters/listener/tls_inspector/boringssl:config"],
    }),
)

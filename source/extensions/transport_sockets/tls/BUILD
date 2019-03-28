licenses(["notice"])  # Apache 2

# Built-in TLS connection transport socket.

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "config",
    hdrs = ["config.h"],
    deps = select({
        "//bazel:openssl": ["//source/extensions/transport_sockets/tls/openssl:config"],
        "//conditions:default": ["//source/extensions/transport_sockets/tls/boringssl:config"],
    }),
)

envoy_cc_library(
    name = "ssl_socket_lib",
    hdrs = ["ssl_socket.h"],
    deps = select({
        "//bazel:openssl": ["//source/extensions/transport_sockets/tls/openssl:ssl_socket_lib"],
        "//conditions:default": ["//source/extensions/transport_sockets/tls/boringssl:ssl_socket_lib"],
    }),
)

envoy_cc_library(
    name = "context_config_lib",
    hdrs = ["context_config_impl.h"],
    deps = select({
        "//bazel:openssl": ["//source/extensions/transport_sockets/tls/openssl:context_config_lib"],
        "//conditions:default": ["//source/extensions/transport_sockets/tls/boringssl:context_config_lib"],
    }),
)

envoy_cc_library(
    name = "context_lib",
    hdrs = [
        "context_impl.h",
        "context_manager_impl.h",
    ],
    deps = select({
        "//bazel:openssl": ["//source/extensions/transport_sockets/tls/openssl:context_lib"],
        "//conditions:default": ["//source/extensions/transport_sockets/tls/boringssl:context_lib"],
    }),
)

envoy_cc_library(
    name = "utility_lib",
    hdrs = ["utility.h"],
    deps = select({
        "//bazel:openssl": ["//source/extensions/transport_sockets/tls/openssl:utility_lib"],
        "//conditions:default": ["//source/extensions/transport_sockets/tls/boringssl:utility_lib"],
    }),
)

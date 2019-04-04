licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_library_internal")

api_proto_library_internal(
    name = "qat_private_key_provider_proto",
    srcs = ["qat.proto"],
    visibility = ["//visibility:public"],
)

envoy_package()

envoy_cc_library(
    name = "qat_private_key_provider_lib",
    srcs = [
        "qat.cc",
        "qat_private_key_provider.cc",
    ],
    hdrs = [
        "qat.h",
        "qat_private_key_provider.h",
    ],
    external_deps = ["ssl"],
    deps = [
        ":qat_private_key_provider_proto_cc",
        "//include/envoy/api:api_interface",
        "//include/envoy/event:dispatcher_interface",
        "//include/envoy/registry",
        "//include/envoy/server:transport_socket_config_interface",
        "//include/envoy/singleton:manager_interface",
        "//include/envoy/ssl/private_key:private_key_config_interface",
        "//include/envoy/ssl/private_key:private_key_interface",
        "//source/common/common:thread_lib",
        "//source/extensions/transport_sockets/tls:utility_lib",
        "@com_intel_qat//:qat",
    ],
)

envoy_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":qat_private_key_provider_lib",
        "//include/envoy/registry",
        "//include/envoy/ssl/private_key:private_key_config_interface",
        "//include/envoy/ssl/private_key:private_key_interface",
        "//source/common/config:utility_lib",
        "//source/common/protobuf:utility_lib",
    ],
)

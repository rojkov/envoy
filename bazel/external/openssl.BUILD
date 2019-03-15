licenses(["notice"])  # Apache 2

load(":genrule_cmd.bzl", "genrule_cmd")

cc_library(
    name = "crypto",
    srcs = [
        "crypto/libcrypto.a",
    ],
    hdrs = glob(["openssl/include/openssl/*.h"]),
    includes = ["openssl/include"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ssl",
    srcs = [
        "ssl/libssl.so",
    ],
    hdrs = glob(["openssl/include/openssl/*.h"]),
    includes = ["openssl/include"],
    visibility = ["//visibility:public"],
    deps = [":crypto"],
)

genrule(
    name = "build",
    srcs = glob(["openssl-OpenSSL_1_1_1b/**"]),
    outs = [
        "crypto/libcrypto.a",
        "ssl/libssl.so",
    ],
    cmd = genrule_cmd("@envoy//bazel/external:openssl.genrule_cmd"),
)

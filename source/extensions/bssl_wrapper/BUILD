licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "bssl_wrapper_lib",
    srcs = [
        "bssl_wrapper.cc",
    ],
    hdrs = [
        "bssl_wrapper.h",
    ],
	defines = ["BORINGSLL_IS_WRAPPED"],
    deps = [
        "@openssl//:ssl",
    ],
)

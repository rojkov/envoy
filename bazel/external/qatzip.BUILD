licenses(["notice"])  # Apache 2

cc_library(
    name = "host-qatzip",
    srcs = [
        "libqatzip.so",
    ],
    visibility = ["//visibility:public"],
    linkstatic=False,
)

alias(
    name = "qatzip",
    actual = "host-qatzip",
    visibility = ["//visibility:public"],
)

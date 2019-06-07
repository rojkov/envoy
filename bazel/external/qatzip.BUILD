licenses(["notice"])  # Apache 2

cc_library(
    name = "qatzip",
    srcs = glob([
        "src/*.c",
        "src/*h",
        "include/*.h",
    ]),
    hdrs = glob(["include/*.h"]),
    includes = [
        "include",
        "src",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_intel_qat//:qat",
        "@com_intel_qat//:usdm_user",
    ],
)

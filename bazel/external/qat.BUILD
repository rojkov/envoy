QAT_DEFINES = [
        '-D ARCH=\\"x86_64\\"',
        "-D USER_SPACE=1",
        '-D ICP_OS_LEVEL=\\"user_space\\"',
        "-D ADF_PLATFORM_ACCELDEV=1",
        "-D QAT_UIO=1",
        '-D OS=\\"linux\\"',
        '-D ICP_OS=\\"linux_2.6\\"',
        ]

QAT_INCLUDE_PATHS = [
        "-I external/com_intel_qat/quickassist/include",
        "-I external/com_intel_qat/quickassist/include/dc",
        "-I external/com_intel_qat/quickassist/include/lac",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/qat_direct/src",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/qat_direct/include",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/include",
        "-I external/com_intel_qat/quickassist/lookaside/firmware/include",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/qat_direct/src/include/user_proxy",
        "-I external/com_intel_qat/quickassist/utilities/osal/include",
        "-I external/com_intel_qat/quickassist/utilities/osal/src/linux/user_space/include",
        "-I external/com_intel_qat/quickassist/qat/drivers/crypto/qat/qat_common",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/qat_direct/src/include/accel_mgr",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/qat_direct/src/include/transport",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/qat_direct/src/include/platform",
        "-I external/com_intel_qat/quickassist/utilities/libusdm_drv",
        "-I external/com_intel_qat/quickassist/utilities/libusdm_drv/linux/include",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/common/include",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/common/crypto/sym/include",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/common/crypto/asym/include",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/common/compression/include",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/common/crypto/asym/rsa",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/common/crypto/kpt/include",
        "-I external/com_intel_qat/quickassist/lookaside/access_layer/src/common/utils",
        ]

QAT_COMMON_HEADERS = glob([
        "quickassist/lookaside/access_layer/include/*.h",
        "quickassist/include/*.h",
        "quickassist/qat/drivers/crypto/qat/qat_common/*.h",
        ])

cc_library(
        name = "qat_direct",
        srcs = glob([
                "quickassist/lookaside/access_layer/src/qat_direct/src/*.c",
                "quickassist/lookaside/access_layer/src/qat_direct/src/*.h",
                ]) + QAT_COMMON_HEADERS,
        hdrs = glob([
                "quickassist/lookaside/access_layer/src/qat_direct/include/*.h",
                "quickassist/lookaside/access_layer/src/qat_direct/src/include/**/*.h",
                ]),
        copts = QAT_INCLUDE_PATHS + QAT_DEFINES,
        deps = [":usdm_user", ":osal"],
        )

cc_library(
        name = "osal",
        srcs = glob([
                "quickassist/utilities/osal/src/linux/user_space/*.c",
                "quickassist/utilities/osal/src/linux/user_space/*.h",
                "quickassist/utilities/osal/src/linux/user_space/openssl/*.c",
                "quickassist/utilities/osal/src/linux/user_space/openssl/*.h",
                "quickassist/utilities/osal/include/*.h",
                ]) + QAT_COMMON_HEADERS,
        hdrs = glob([
                "quickassist/utilities/osal/src/linux/user_space/include/*.h",
                "quickassist/utilities/osal/include/*.h",
                ]),
        copts = QAT_INCLUDE_PATHS + QAT_DEFINES,
        )

cc_library(
        name = "usdm_user",
        srcs = glob([
                "quickassist/utilities/libusdm_drv/linux/user_space/*.c",
                "quickassist/utilities/libusdm_drv/linux/user_space/*.h",
                "quickassist/utilities/libusdm_drv/qae_mem.h",
                "quickassist/utilities/libusdm_drv/linux/user_space/*.h",
                ]) + QAT_COMMON_HEADERS,
        hdrs = glob([
                "quickassist/utilities/libusdm_drv/linux/include/*.h",
                "quickassist/utilities/libusdm_drv/qae_mem.h"
                ]),
        copts = QAT_INCLUDE_PATHS + QAT_DEFINES,
        )

QAT_LAC_SRCS = glob([
                "quickassist/lookaside/access_layer/src/common/**/*.c",
                "quickassist/lookaside/access_layer/src/common/**/*.h",
                "quickassist/lookaside/access_layer/src/common/**/**/*.c",
                "quickassist/lookaside/access_layer/src/common/**/include/*.h",
                "quickassist/lookaside/access_layer/src/common/include/*.h",
                "quickassist/lookaside/access_layer/src/user/*.c",
                "quickassist/lookaside/access_layer/src/common/crypto/sym/include/*.h",
                "quickassist/lookaside/firmware/include/*.h",
                "quickassist/include/dc/*.h",
                "quickassist/include/lac/*h",
                ]) + QAT_COMMON_HEADERS

# remove stubs from build
QAT_LAC_SRCS.remove("quickassist/lookaside/access_layer/src/common/crypto/stubs/lac_stubs.c")

# remove precomputed values to prevent symbol clash
QAT_LAC_SRCS.remove("quickassist/lookaside/access_layer/src/common/crypto/sym/lac_sym_hash_hw_precomputes.c")

cc_library(
        name = "qat",
        srcs = QAT_LAC_SRCS,
        hdrs = glob([
                "quickassist/include/*.h",
                "quickassist/include/**/*.h",
                "quickassist/lookaside/access_layer/include/*.h",
                ]),
        copts = QAT_INCLUDE_PATHS + QAT_DEFINES,
        deps = [ ":osal", ":usdm_user", ":qat_direct", "@org_freedesktop_systemd//:udev" ],
        visibility = ["//visibility:public"],
        # linkstatic = True,
        alwayslink = True,
        )

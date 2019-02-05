UDEV_DEFINES = [
        "-D_GNU_SOURCE=1",
        "-DSIZEOF_PID_T=4",
        "-DSIZEOF_UID_T=4",
        "-DSIZEOF_GID_T=4",
        "-DSIZEOF_DEV_T=8",
        "-DSIZEOF_INO_T=8",
        "-DSIZEOF_TIME_T=8",
        "-DSIZEOF_RLIM_T=8",
        "-DHAVE_SECURE_GETENV=1",
        "-DHAVE_REALLOCARRAY=1",
        "-DHAVE_STRUCT_STATX=1",
        "-DHAVE_NAME_TO_HANDLE_AT=1",
        "-DUSE_SYS_RANDOM_H=1",
        "-DHAVE_GETRANDOM=1",
        "-DSYSTEM_UID_MAX=999",
        "-DSYSTEM_GID_MAX=999",
        "-DGPERF_LEN_TYPE=size_t",
        "-DDYNAMIC_UID_MIN=61184",
        "-DDYNAMIC_UID_MAX=65519",
        '-DFALLBACK_HOSTNAME=\\"localhost\\"',
        "-DTIME_EPOCH=1538139994",
        "-DDEFAULT_HIERARCHY=CGROUP_UNIFIED_SYSTEMD",
        '-DDEFAULT_HIERARCHY_NAME=\\"hybrid\\"',
        '-DUDEVLIBEXECDIR=\\"/lib/udev\\"',
        '-DNOBODY_USER_NAME=\\"nobody\\"',
        '-DNOBODY_GROUP_NAME=\\"nobody\\"',
        '-DGETTEXT_PACKAGE=\\"systemd\\"',
        '-DPACKAGE_STRING=\\"systemd\ 240\\"',
        '-DPACKAGE_VERSION=\\"240\\"',
        '-DHAVE_IFLA_INET6_ADDR_GEN_MODE=1',
        '-DHAVE_IFLA_IPVLAN_MODE=1',
        '-DHAVE_IFLA_GENEVE_TOS=1',
        '-DHAVE_IFLA_BR_MAX_AGE=1',
        '-DHAVE_IFLA_VRF_TABLE=1',
        ]

UDEV_COPTS = [
        "-I external/org_freedesktop_systemd/src/basic",
        "-I external/org_freedesktop_systemd/src/shared",
        "-I external/org_freedesktop_systemd/src/libsystemd/sd-hwdb",
        "-I external/org_freedesktop_systemd/src/systemd",
        "-I external/org_freedesktop_systemd/src/libsystemd/sd-device",
        "-I external/org_freedesktop_systemd/$(GENDIR)",
        ]

UDEV_SRCS = glob([
        "src/libudev/*.c",
        "src/libudev/*.h",
        "src/systemd/sd-hwdb.*",
        "src/systemd/_sd-common.*",
        "src/systemd/sd-device.*",
        "src/systemd/sd-messages.*",
        "src/systemd/sd-id128.*",
        "src/systemd/sd-event.*",
        "src/systemd/sd-daemon.*",
        "src/basic/*.h",
        "src/basic/*.c",
        "src/shared/serialize.*",
        "src/shared/fdset.*",
        "src/libsystemd/sd-device/device-monitor.*",
        "src/libsystemd/sd-device/device-util.*",
        "src/libsystemd/sd-device/device-private.*",
        "src/libsystemd/sd-device/device-enumerator-private.*",
        "src/libsystemd/sd-device/device-internal.h",
        "src/libsystemd/sd-hwdb/hwdb-util.*",
        "src/libsystemd/sd-hwdb/hwdb-internal.*",
        "src/libsystemd/sd-device/sd-device.c",
        "src/libsystemd/sd-device/device-monitor-private.h",
        "src/libsystemd/sd-event/sd-event.*",
        "src/libsystemd/sd-event/event-source.h",
        "src/libsystemd/sd-id128/sd-id128.c",
        "src/libsystemd/sd-id128/id128-util.*",
        "src/libsystemd/sd-daemon/sd-daemon.c",
        "src/shared/enable-mempool.c",
        ])

genrule(
        name = "arphrd-header",
        srcs = glob([ "src/basic/generate-arphrd-list.sh", "tools/generate-gperfs.py", "src/basic/arphrd-to-name.awk", "src/basic/missing.h", "src/basic/missing_*.h" ]),
        outs = ["arphrd-list.txt", "arphrd-from-name.gperf", "arphrd-from-name.h", "arphrd-to-name.h"],
        cmd = ' \
        $(location src/basic/generate-arphrd-list.sh) \"gcc -E\" $(location src/basic/missing.h) $(location src/basic/missing_network.h) > $(location arphrd-list.txt) \
        && $(location tools/generate-gperfs.py) arphrd ARPHRD_ $(location arphrd-list.txt) > $(location arphrd-from-name.gperf) \
        && gperf -L ANSI-C -t --ignore-case -N lookup_arphrd -H hash_arphrd_name -p -C $(location arphrd-from-name.gperf) --output-file=$(location arphrd-from-name.h) \
        && awk -f $(location src/basic/arphrd-to-name.awk) $(location arphrd-list.txt) > $(location arphrd-to-name.h) \
        ',
        )

genrule(
        name = "af-header",
        srcs = glob([ "src/basic/generate-af-list.sh", "tools/generate-gperfs.py", "src/basic/af-to-name.awk", "src/basic/missing.h", "src/basic/missing_*.h" ]),
        outs = ["af-list.txt", "af-from-name.gperf", "af-from-name.h", "af-to-name.h"],
        cmd = ' \
        $(location src/basic/generate-af-list.sh) \"gcc -E\" $(location src/basic/missing.h) $(location src/basic/missing_socket.h) > $(location af-list.txt) \
        && $(location tools/generate-gperfs.py) af \'\' $(location af-list.txt) > $(location af-from-name.gperf) \
        && gperf -L ANSI-C -t --ignore-case -N lookup_af -H hash_af_name -p -C $(location af-from-name.gperf) --output-file=$(location af-from-name.h) \
        && awk -f $(location src/basic/af-to-name.awk) $(location af-list.txt) > $(location af-to-name.h) \
        ',
        )

genrule(
        name = "errno-header",
        srcs = [ "src/basic/generate-errno-list.sh", "tools/generate-gperfs.py", "src/basic/errno-to-name.awk" ],
        outs = ["errno-list.txt", "errno-from-name.gperf", "errno-from-name.h", "errno-to-name.h"],
        cmd = ' \
        $(location src/basic/generate-errno-list.sh) \"gcc -E\" > $(location errno-list.txt) \
        && $(location tools/generate-gperfs.py) errno \'\' $(location errno-list.txt) > $(location errno-from-name.gperf) \
        && gperf -L ANSI-C -t --ignore-case -N lookup_errno -H hash_errno_name -p -C $(location errno-from-name.gperf) --output-file=$(location errno-from-name.h) \
        && awk -f $(location src/basic/errno-to-name.awk) $(location errno-list.txt) > $(location errno-to-name.h) \
        ',
        )

genrule(
        name = "cap-header",
        srcs = glob([ "src/basic/generate-cap-list.sh", "tools/generate-gperfs.py", "src/basic/cap-to-name.awk", "src/basic/missing.h", "src/basic/missing_*.h" ]),
        outs = ["cap-list.txt", "cap-from-name.gperf", "cap-from-name.h", "cap-to-name.h"],
        cmd = ' \
        $(location src/basic/generate-cap-list.sh) \"gcc -E -DHAVE_STRUCT_STATX -DHAVE_SECURE_GETENV=1\" $(location src/basic/missing.h) $(location src/basic/missing.h) > $(location cap-list.txt) \
        && $(location tools/generate-gperfs.py) capability \'\' $(location cap-list.txt) > $(location cap-from-name.gperf) \
        && gperf -L ANSI-C -t --ignore-case -N lookup_capability -H hash_capability_name -C $(location cap-from-name.gperf) --output-file=$(location cap-from-name.h) \
        && awk -f $(location src/basic/cap-to-name.awk) $(location cap-list.txt) > $(location cap-to-name.h) \
        ',
        )

cc_library(
        name = "udev",
        srcs = UDEV_SRCS + [
                ":arphrd-from-name.h", ":arphrd-to-name.h",
                ":af-from-name.h", ":af-to-name.h",
                ":errno-from-name.h", ":errno-to-name.h",
                ":cap-from-name.h", ":cap-to-name.h",
                ],
        copts = UDEV_COPTS + UDEV_DEFINES,
        deps = ["@org_kernel_util_linux//:mount"],
        hdrs = ["src/libudev/libudev.h" ],
        visibility = ["//visibility:public"],
        )

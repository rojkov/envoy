licenses(["notice"])  # Apache 2

load(":genrule_cmd.bzl", "genrule_cmd")

cc_library(
    name = "crypto",
    srcs = [
        "lib/libcrypto.a",
    ],
    hdrs = glob(["com_github_openssl/include/openssl/*.h"]),
    includes = ["com_github_openssl/include"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ssl",
    srcs = [
        "lib/libssl.a",
    ],
    visibility = ["//visibility:public"],
    deps = [":crypto"],
)

genrule(
    name = "build",
    srcs = glob(["openssl-OpenSSL_1_1_1b/**"]),
    outs = [
        "include/openssl/pkcs7.h",
        "include/openssl/cterr.h",
        "include/openssl/tls1.h",
        "include/openssl/pkcs12.h",
        "include/openssl/rand_drbg.h",
        "include/openssl/rc2.h",
        "include/openssl/blowfish.h",
        "include/openssl/seed.h",
        "include/openssl/opensslconf.h",
        "include/openssl/buffererr.h",
        "include/openssl/tserr.h",
        "include/openssl/x509err.h",
        "include/openssl/asyncerr.h",
        "include/openssl/buffer.h",
        "include/openssl/ecerr.h",
        "include/openssl/sha.h",
        "include/openssl/engine.h",
        "include/openssl/bioerr.h",
        "include/openssl/kdf.h",
        "include/openssl/conf_api.h",
        "include/openssl/sslerr.h",
        "include/openssl/err.h",
        "include/openssl/x509_vfy.h",
        "include/openssl/md4.h",
        "include/openssl/ts.h",
        "include/openssl/pkcs7err.h",
        "include/openssl/modes.h",
        "include/openssl/md2.h",
        "include/openssl/pemerr.h",
        "include/openssl/ecdh.h",
        "include/openssl/dh.h",
        "include/openssl/rand.h",
        "include/openssl/rc5.h",
        "include/openssl/srp.h",
        "include/openssl/aes.h",
        "include/openssl/cmserr.h",
        "include/openssl/mdc2.h",
        "include/openssl/asn1_mac.h",
        "include/openssl/lhash.h",
        "include/openssl/randerr.h",
        "include/openssl/dherr.h",
        "include/openssl/x509v3.h",
        "include/openssl/camellia.h",
        "include/openssl/crypto.h",
        "include/openssl/obj_mac.h",
        "include/openssl/kdferr.h",
        "include/openssl/hmac.h",
        "include/openssl/ocsp.h",
        "include/openssl/asn1err.h",
        "include/openssl/idea.h",
        "include/openssl/bio.h",
        "include/openssl/ssl.h",
        "include/openssl/objectserr.h",
        "include/openssl/ossl_typ.h",
        "include/openssl/cmac.h",
        "include/openssl/pem.h",
        "include/openssl/ecdsa.h",
        "include/openssl/comp.h",
        "include/openssl/asn1.h",
        "include/openssl/ebcdic.h",
        "include/openssl/symhacks.h",
        "include/openssl/opensslv.h",
        "include/openssl/cast.h",
        "include/openssl/bnerr.h",
        "include/openssl/pkcs12err.h",
        "include/openssl/ssl2.h",
        "include/openssl/x509.h",
        "include/openssl/e_os2.h",
        "include/openssl/comperr.h",
        "include/openssl/ui.h",
        "include/openssl/ocsperr.h",
        "include/openssl/srtp.h",
        "include/openssl/md5.h",
        "include/openssl/ssl3.h",
        "include/openssl/safestack.h",
        "include/openssl/stack.h",
        "include/openssl/evp.h",
        "include/openssl/store.h",
        "include/openssl/bn.h",
        "include/openssl/pem2.h",
        "include/openssl/txt_db.h",
        "include/openssl/cms.h",
        "include/openssl/conf.h",
        "include/openssl/whrlpool.h",
        "include/openssl/rsa.h",
        "include/openssl/des.h",
        "include/openssl/dsa.h",
        "include/openssl/evperr.h",
        "include/openssl/conferr.h",
        "include/openssl/ripemd.h",
        "include/openssl/ec.h",
        "include/openssl/dsaerr.h",
        "include/openssl/storeerr.h",
        "include/openssl/x509v3err.h",
        "include/openssl/objects.h",
        "include/openssl/engineerr.h",
        "include/openssl/rc4.h",
        "include/openssl/cryptoerr.h",
        "include/openssl/ct.h",
        "include/openssl/async.h",
        "include/openssl/dtls1.h",
        "include/openssl/asn1t.h",
        "include/openssl/uierr.h",
        "include/openssl/rsaerr.h",
        "lib/libssl.so.1.1",
        "lib/libcrypto.so",
        "lib/engines-1.1/afalg.so",
        "lib/engines-1.1/padlock.so",
        "lib/engines-1.1/capi.so",
        "lib/libssl.so",
        "lib/pkgconfig/openssl.pc",
        "lib/pkgconfig/libcrypto.pc",
        "lib/pkgconfig/libssl.pc",
        "lib/libcrypto.so.1.1",
        "lib/libssl.a",
        "lib/libcrypto.a",
    ],
    cmd = genrule_cmd("@envoy//bazel/external:openssl.genrule_cmd"),
)

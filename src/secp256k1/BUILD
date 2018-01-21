licenses(["notice"])

secp256k1_copts = [
    "-W",
    "-std=c89",
    "-pedantic",
    "-Wall",
    "-Wextra",
    "-Wcast-align",
    "-Wnested-externs",
    "-Wshadow",
    "-Wstrict-prototypes",
    "-Wno-unused-function",
    "-Wno-long-long",
    "-Wno-overlength-strings",
    "-fvisibility=hidden",
    "-Isrc/secp256k1",
    "-I$(GENDIR)/src/secp256k1/src",
    "-Isrc/secp256k1/src",
    "-DHAVE_CONFIG_H",
    "-DSECP256K1_BUILD",
]

cc_binary(
    name = "gen_context",
    srcs = [
        "src/gen_context.c",
        "include/secp256k1.h",
    ] + glob(["src/*.h"]),
    includes = ["."],
)

genrule(
    name = "ecmult_static_context",
    outs = ["src/ecmult_static_context.h"],
    # The mkdir dance here is because gen_context assumes that it should
    # generate the header in the source tree and thus writes to
    # src/ecmult_static_context.h, which does not work for this case.
    cmd = "mkdir gen_context_tmp && (cd gen_context_tmp && mkdir src && ../$(location :gen_context)) && mv gen_context_tmp/src/ecmult_static_context.h $@ && rmdir gen_context_tmp/src && rmdir gen_context_tmp",
    tools = [":gen_context"],
)

cc_library(
    name = "secp256k1",
    visibility = ["//visibility:public"],
    includes = ["include"],
    hdrs = [
        "include/secp256k1.h",
        "include/secp256k1_ecdh.h",
        "include/secp256k1_recovery.h",
    ],
    srcs = [
        "src/basic-config.h",
        "src/bench.h",
        "src/ecdsa.h",
        "src/ecdsa_impl.h",
        "src/eckey.h",
        "src/eckey_impl.h",
        "src/ecmult.h",
        "src/ecmult_const.h",
        "src/ecmult_const_impl.h",
        "src/ecmult_gen.h",
        "src/ecmult_gen_impl.h",
        "src/ecmult_impl.h",
        "src/ecmult_static_context.h",
        "src/field.h",
        "src/field_10x26.h",
        "src/field_10x26_impl.h",
        "src/field_5x52.h",
        "src/field_5x52_asm_impl.h",
        "src/field_5x52_impl.h",
        "src/field_5x52_int128_impl.h",
        "src/field_impl.h",
        "src/group.h",
        "src/group_impl.h",
        "src/hash.h",
        "src/hash_impl.h",
        "src/libsecp256k1-config.h",
        "src/modules/ecdh/main_impl.h",
        "src/modules/recovery/main_impl.h",
        "src/num.h",
        "src/num_gmp.h",
        "src/num_gmp_impl.h",
        "src/num_impl.h",
        "src/scalar.h",
        "src/scalar_4x64.h",
        "src/scalar_4x64_impl.h",
        "src/scalar_8x32.h",
        "src/scalar_8x32_impl.h",
        "src/scalar_impl.h",
        "src/scalar_low.h",
        "src/scalar_low_impl.h",
        "src/secp256k1.c",
        "src/testrand.h",
        "src/testrand_impl.h",
        "src/util.h",
    ],
    copts = secp256k1_copts,
    deps = [
        "@libgmp//:gmp",
    ],
)

cc_library(
    name = "test_sources",
    srcs = [
        "src/util.h",
    ],
    textual_hdrs = [
        "contrib/lax_der_parsing.c",
        "contrib/lax_der_parsing.h",
        "contrib/lax_der_privatekey_parsing.c",
        "contrib/lax_der_privatekey_parsing.h",
        "src/secp256k1.c",
    ],
    includes = ["contrib"],
)

cc_test(
    name = "tests",
    srcs = [
        "src/tests.c",
        "src/modules/ecdh/tests_impl.h",
        "src/modules/recovery/tests_impl.h",
    ],
    deps = [
        ":secp256k1",
        ":test_sources",
    ],
    copts = secp256k1_copts + ["-DVERIFY"],
    linkopts = ["-lpthread"],
)

cc_test(
    name = "tests_exhaustive",
    size = "small",
    srcs = [
        "src/tests_exhaustive.c",
    ],
    deps = [
        ":secp256k1",
        ":test_sources",
    ],
    copts = secp256k1_copts + ["-DVERIFY"],
    linkopts = ["-lpthread"],
)

libsecp256k1_config_h_contents = r"""
/* src/libsecp256k1-config.h.  Generated from libsecp256k1-config.h.in by configure.  */
/* src/libsecp256k1-config.h.in.  Generated from configure.ac by autoheader.  */

#ifndef LIBSECP256K1_CONFIG_H

#define LIBSECP256K1_CONFIG_H

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define this symbol to compile out all VERIFY code */
/* #undef COVERAGE */

/* Define this symbol to enable the ECDH module */
/* #undef ENABLE_MODULE_ECDH */

/* Define this symbol to enable the ECDSA pubkey recovery module */
#define ENABLE_MODULE_RECOVERY 1

/* Define this symbol if OpenSSL EC functions are available */
/* #undef ENABLE_OPENSSL_TESTS */

/* Define this symbol if __builtin_expect is available */
#define HAVE_BUILTIN_EXPECT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define this symbol if libcrypto is installed */
#define HAVE_LIBCRYPTO 1

/* Define this symbol if libgmp is installed */
#define HAVE_LIBGMP 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if the system has the type `__int128'. */
#define HAVE___INT128 1

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "libsecp256k1"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "libsecp256k1"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libsecp256k1 0.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libsecp256k1"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.1"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define this symbol to enable x86_64 assembly optimizations */
#define USE_ASM_X86_64 1

/* Define this symbol to use a statically generated ecmult table */
#define USE_ECMULT_STATIC_PRECOMPUTATION 1

/* Define this symbol to use endomorphism optimization */
/* #undef USE_ENDOMORPHISM */

/* Define this symbol if an external (non-inline) assembly implementation is
   used */
/* #undef USE_EXTERNAL_ASM */

/* Define this symbol to use the FIELD_10X26 implementation */
/* #undef USE_FIELD_10X26 */

/* Define this symbol to use the FIELD_5X52 implementation */
#define USE_FIELD_5X52 1

/* Define this symbol to use the native field inverse implementation */
#define USE_FIELD_INV_BUILTIN 1

/* Define this symbol to use the num-based field inverse implementation */
/* #undef USE_FIELD_INV_NUM */

/* Define this symbol to use the gmp implementation for num */
#define USE_NUM_GMP 1

/* Define this symbol to use no num implementation */
/* #undef USE_NUM_NONE */

/* Define this symbol to use the 4x64 scalar implementation */
#define USE_SCALAR_4X64 1

/* Define this symbol to use the 8x32 scalar implementation */
/* #undef USE_SCALAR_8X32 */

/* Define this symbol to use the native scalar inverse implementation */
#define USE_SCALAR_INV_BUILTIN 1

/* Define this symbol to use the num-based scalar inverse implementation */
/* #undef USE_SCALAR_INV_NUM */

/* Version number of package */
#define VERSION "0.1"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

#endif /*LIBSECP256K1_CONFIG_H*/
"""
genrule(
    name = 'libsecpk1_config_h',
    outs = ['src/libsecp256k1-config.h'],
    cmd = "cat > $@ << 'BAZEL_EOF'\n" + libsecp256k1_config_h_contents.replace('$', '$$') + "\nBAZEL_EOF",
)

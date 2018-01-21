cc_library(
    name = "bitcoin-config",
    visibility = ["//visibility:public"],
    hdrs = ["bitcoin-config.h"],
    includes = ["."],
    include_prefix = "config",
    defines = ["HAVE_CONFIG_H"],
)

bitcoin_config_contents = r"""
#ifndef BITCOIN_CONFIG_H
#define BITCOIN_CONFIG_H

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Version Build */
#define CLIENT_VERSION_BUILD 50

/* Version is release */
#define CLIENT_VERSION_IS_RELEASE true

/* Major version */
#define CLIENT_VERSION_MAJOR 1

/* Minor version */
#define CLIENT_VERSION_MINOR 0

/* Build revision */
#define CLIENT_VERSION_REVISION 13

/* Version is release */
#define COPYRIGHT_YEAR 2017

/* Define to 1 to enable mining functions */
#define ENABLE_MINING 1

/* Define to 1 to enable Proton functions */
#define ENABLE_PROTON 1

/* Define to 1 to enable Rust language dependent functions */
/* #undef ENABLE_RUST */

/* Define to 1 to enable wallet functions */
#define ENABLE_WALLET 1

/* Define to 1 to enable ZMQ functions */
#define ENABLE_ZMQ 1

/* parameter and return value type for __fdelt_chk */
/* #undef FDELT_TYPE */

/* Define to 1 if you have the <byteswap.h> header file. */
#define HAVE_BYTESWAP_H 1

/* Define this symbol if the consensus lib has been built */
#define HAVE_CONSENSUS_LIB 1

/* Define to 1 if you have the declaration of `be16toh', and to 0 if you
   don't. */
#define HAVE_DECL_BE16TOH 1

/* Define to 1 if you have the declaration of `be32toh', and to 0 if you
   don't. */
#define HAVE_DECL_BE32TOH 1

/* Define to 1 if you have the declaration of `be64toh', and to 0 if you
   don't. */
#define HAVE_DECL_BE64TOH 1

/* Define to 1 if you have the declaration of `bswap_16', and to 0 if you
   don't. */
#define HAVE_DECL_BSWAP_16 1

/* Define to 1 if you have the declaration of `bswap_32', and to 0 if you
   don't. */
#define HAVE_DECL_BSWAP_32 1

/* Define to 1 if you have the declaration of `bswap_64', and to 0 if you
   don't. */
#define HAVE_DECL_BSWAP_64 1

/* Define to 1 if you have the declaration of `htobe16', and to 0 if you
   don't. */
#define HAVE_DECL_HTOBE16 1

/* Define to 1 if you have the declaration of `htobe32', and to 0 if you
   don't. */
#define HAVE_DECL_HTOBE32 1

/* Define to 1 if you have the declaration of `htobe64', and to 0 if you
   don't. */
#define HAVE_DECL_HTOBE64 1

/* Define to 1 if you have the declaration of `htole16', and to 0 if you
   don't. */
#define HAVE_DECL_HTOLE16 1

/* Define to 1 if you have the declaration of `htole32', and to 0 if you
   don't. */
#define HAVE_DECL_HTOLE32 1

/* Define to 1 if you have the declaration of `htole64', and to 0 if you
   don't. */
#define HAVE_DECL_HTOLE64 1

/* Define to 1 if you have the declaration of `le16toh', and to 0 if you
   don't. */
#define HAVE_DECL_LE16TOH 1

/* Define to 1 if you have the declaration of `le32toh', and to 0 if you
   don't. */
#define HAVE_DECL_LE32TOH 1

/* Define to 1 if you have the declaration of `le64toh', and to 0 if you
   don't. */
#define HAVE_DECL_LE64TOH 1

/* Define to 1 if you have the declaration of `strnlen', and to 0 if you
   don't. */
#define HAVE_DECL_STRNLEN 1

/* Define to 1 if you have the <endian.h> header file. */
#define HAVE_ENDIAN_H 1

/* Define to 1 if the system has the `dllexport' function attribute */
/* #undef HAVE_FUNC_ATTRIBUTE_DLLEXPORT */

/* Define to 1 if the system has the `dllimport' function attribute */
/* #undef HAVE_FUNC_ATTRIBUTE_DLLIMPORT */

/* Define to 1 if the system has the `visibility' function attribute */
#define HAVE_FUNC_ATTRIBUTE_VISIBILITY 1

/* Define this symbol if you have getaddrinfo_a */
#define HAVE_GETADDRINFO_A 1

/* Define this symbol if you have inet_pton */
#define HAVE_INET_PTON 1

/* Define this symbol if you have MSG_NOSIGNAL */
#define HAVE_MSG_NOSIGNAL 1

/* Define to 1 if you have the <sys/endian.h> header file. */
/* #undef HAVE_SYS_ENDIAN_H */

/* Define to 1 if you have the <sys/prctl.h> header file. */
#define HAVE_SYS_PRCTL_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define if the visibility attribute is supported. */
#define HAVE_VISIBILITY_ATTRIBUTE 1

/* Define to 1 if strerror_r returns char *. */
#define STRERROR_R_CHAR_P 1

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

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

#endif // BITCOIN_CONFIG_H
"""
genrule(
    name = "bitcoin-config-header",
    outs = ["bitcoin-config.h"],
    cmd = "cat > \$@ << 'BAZEL_EOF'\n" + bitcoin_config_contents.replace("$", "$$") + "\nBAZEL_EOF",
)

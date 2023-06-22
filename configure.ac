dnl require autoconf 2.60 (AS_ECHO/AS_ECHO_N)
AC_PREREQ([2.60])
define(_CLIENT_VERSION_MAJOR, 5)
define(_CLIENT_VERSION_MINOR, 6)
define(_CLIENT_VERSION_REVISION, 1)
define(_CLIENT_VERSION_BUILD, 50)
define(_ZC_BUILD_VAL, m4_if(m4_eval(_CLIENT_VERSION_BUILD < 25), 1, m4_incr(_CLIENT_VERSION_BUILD), m4_eval(_CLIENT_VERSION_BUILD < 50), 1, m4_eval(_CLIENT_VERSION_BUILD - 24), m4_eval(_CLIENT_VERSION_BUILD == 50), 1, , m4_eval(_CLIENT_VERSION_BUILD - 50)))
define(_CLIENT_VERSION_SUFFIX, m4_if(m4_eval(_CLIENT_VERSION_BUILD < 25), 1, _CLIENT_VERSION_REVISION-beta$1, m4_eval(_CLIENT_VERSION_BUILD < 50), 1, _CLIENT_VERSION_REVISION-rc$1, m4_eval(_CLIENT_VERSION_BUILD == 50), 1, _CLIENT_VERSION_REVISION, _CLIENT_VERSION_REVISION-$1)))
define(_CLIENT_VERSION_IS_RELEASE, true)
define(_COPYRIGHT_YEAR, 2023)
AC_INIT([Zcash],[_CLIENT_VERSION_MAJOR._CLIENT_VERSION_MINOR._CLIENT_VERSION_SUFFIX(_ZC_BUILD_VAL)],[https://github.com/zcash/zcash/issues],[zcash])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([src/config/bitcoin-config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])

BITCOIN_DAEMON_NAME=zcashd
BITCOIN_CLI_NAME=zcash-cli
BITCOIN_TX_NAME=zcash-tx

dnl Unless the user specified ARFLAGS, force it to be cr
AC_ARG_VAR(ARFLAGS, [Flags for the archiver, defaults to <cr> if not set])
if test "x${ARFLAGS+set}" != "xset"; then
  ARFLAGS="cr"
fi

AC_CANONICAL_HOST

AH_TOP([#ifndef BITCOIN_CONFIG_H])
AH_TOP([#define BITCOIN_CONFIG_H])
AH_BOTTOM([#endif //BITCOIN_CONFIG_H])

dnl faketime breaks configure and is only needed for make. Disable it here.
unset FAKETIME

dnl Automake init set-up and checks
AM_INIT_AUTOMAKE([no-define subdir-objects foreign tar-pax])

dnl faketime messes with timestamps and causes configure to be re-run.
dnl --disable-maintainer-mode can be used to bypass this.
AM_MAINTAINER_MODE([enable])

dnl make the compilation flags quiet unless V=1 is used
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Compiler checks (here before libtool).
if test "x${CXXFLAGS+set}" = "xset"; then
  CXXFLAGS_overridden=yes
else
  CXXFLAGS_overridden=no
fi

AC_PROG_CXX
m4_ifdef([AC_PROG_OBJCXX],[AC_PROG_OBJCXX])

dnl By default, libtool for mingw refuses to link static libs into a dll for
dnl fear of mixing pic/non-pic objects, and import/export complications. Since
dnl we have those under control, re-enable that functionality.
case $host in
  *mingw*)
     lt_cv_deplibs_check_method="pass_all"
  ;;
esac
dnl Require C++17 compiler (no GNU extensions)
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])
dnl Check if -latomic is required for <std::atomic>
CHECK_ATOMIC

dnl Libtool init checks.
LT_INIT([pic-only])

dnl Zcash macros for required programs. Configure exits with error if
dnl they are not detected.
AC_DEFUN(
  ZC_REQUIRE_TOOL,
  [
    AC_PATH_TOOL($1, $2, $2_notfound)
    if test x${$1} = x$2_notfound
    then
      AC_MSG_ERROR("Required tool $2 was not found")
    fi
  ]
)

AC_DEFUN(
  ZC_REQUIRE_PROG,
  [
    AC_PATH_PROG($1, $2, $2_notfound)
    if test x${$1} = x$2_notfound
    then
      AC_MSG_ERROR("Required program $2 was not found")
    fi
  ]
)

dnl Check/return PATH for base programs.
dnl Required tools and programs:
ZC_REQUIRE_TOOL(AR, ar)
ZC_REQUIRE_TOOL(RANLIB, ranlib)
ZC_REQUIRE_TOOL(STRIP, strip)
ZC_REQUIRE_PROG([GIT], [git])
ZC_REQUIRE_PROG(RUSTC, rustc)
ZC_REQUIRE_PROG(CARGO, cargo)
ZC_REQUIRE_PROG(CXXBRIDGE, cxxbridge)
dnl This one is still optional and checked by complicated logic below:
AC_PATH_PROG(CCACHE,ccache)
dnl This one is not currently used anywhere, thus not required:
AC_PATH_PROG(XGETTEXT,xgettext)
ZC_REQUIRE_PROG(HEXDUMP,hexdump)
AC_PATH_PROG(READELF,readelf)
ZC_REQUIRE_TOOL(CPPFILT,c++filt)

AC_ARG_ENABLE([online-rust],
  [AS_HELP_STRING([--enable-online-rust],
  [enable Rust to fetch dependencies from the network (default is to use vendored dependencies)])],
  [enable_online_rust=$enableval],
  [enable_online_rust=no])
AM_CONDITIONAL(ENABLE_ONLINE_RUST, test "$enable_online_rust" != no)

# Enable wallet
AC_ARG_ENABLE([wallet],
  [AS_HELP_STRING([--disable-wallet],
  [disable wallet (enabled by default)])],
  [enable_wallet=$enableval],
  [enable_wallet=yes])

AC_ARG_ENABLE([mining],
  [AS_HELP_STRING([--enable-mining],
  [enable mining (default is yes)])],
  [enable_mining=$enableval],
  [enable_mining=yes])

AC_ARG_ENABLE(tests,
    AS_HELP_STRING([--disable-tests],[do not compile tests (default is to compile)]),
    [use_tests=$enableval],
    [use_tests=yes])

AC_ARG_ENABLE(bench,
    AS_HELP_STRING([--enable-bench],[compile benchmarks (default is yes)]),
    [use_bench=$enableval],
    [use_bench=yes])

AC_ARG_ENABLE([fuzz-main],
  [AS_HELP_STRING([--enable-fuzz-main],
  [Replace main() with a stub for fuzzing (default is no)])],
  [use_fuzz_main=$enableval],
  [use_fuzz_main=no])

AC_ARG_ENABLE([hardening],
  [AS_HELP_STRING([--disable-hardening],
  [do not attempt to harden the resulting executables (default is to harden when possible)])],
  [use_hardening=$enableval],
  [use_hardening=auto])

AC_ARG_ENABLE([reduce-exports],
  [AS_HELP_STRING([--enable-reduce-exports],
  [attempt to reduce exported symbols in the resulting executables (default is no)])],
  [use_reduce_exports=$enableval],
  [use_reduce_exports=no])

AC_ARG_ENABLE([ccache],
  [AS_HELP_STRING([--disable-ccache],
  [do not use ccache for building (default is to use if found)])],
  [use_ccache=$enableval],
  [use_ccache=auto])

AC_ARG_ENABLE([lcov],
  [AS_HELP_STRING([--enable-lcov],
  [enable lcov testing (default is no)])],
  [use_lcov=yes],
  [use_lcov=no])

AC_ARG_ENABLE([glibc-back-compat],
  [AS_HELP_STRING([--enable-glibc-back-compat],
  [enable backwards compatibility with glibc])],
  [use_glibc_compat=$enableval],
  [use_glibc_compat=no])

AC_ARG_ENABLE([asm],
  [AS_HELP_STRING([--disable-asm],
  [disable assembly routines (enabled by default)])],
  [use_asm=$enableval],
  [use_asm=yes])

if test "x$use_asm" = xyes; then
  AC_DEFINE(USE_ASM, 1, [Define this symbol to build in assembly routines])
fi

AC_ARG_ENABLE([zmq],
  [AS_HELP_STRING([--disable-zmq],
  [disable ZMQ notifications])],
  [use_zmq=$enableval],
  [use_zmq=yes])

AC_ARG_WITH([protoc-bindir],[AS_HELP_STRING([--with-protoc-bindir=BIN_DIR],[specify protoc bin path])], [protoc_bin_path=$withval], [])

AC_ARG_ENABLE(man,
    [AS_HELP_STRING([--disable-man],
                    [do not install man pages (default is to install)])],,
    enable_man=yes)
AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" != no)

# Enable debug
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
                    [use compiler flags and macros suited for debugging (default is no)])],
    [enable_debug=$enableval],
    [enable_debug=no])

# Enable different -fsanitize options
AC_ARG_WITH([sanitizers],
    [AS_HELP_STRING([--with-sanitizers],
                    [comma separated list of extra sanitizers to build with (default is none enabled)])],
    [use_sanitizers=$withval])

# Enable gprof profiling
AC_ARG_ENABLE([gprof],
    [AS_HELP_STRING([--enable-gprof],
                    [use gprof profiling compiler flags (default is no)])],
    [enable_gprof=$enableval],
    [enable_gprof=no])

# Turn warnings into errors
AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror],
                    [Treat all compiler warnings as errors (default is yes)])],
    [enable_werror=$enableval],
    [enable_werror=yes])

AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Werror],[CXXFLAG_WERROR="-Werror"],[CXXFLAG_WERROR=""])

if test "x$enable_debug" = xyes; then
  # Clear default -g -O2 flags
  if test "x$CXXFLAGS_overridden" = xno; then
	CXXFLAGS=""
  fi

  # Disable all optimizations
  AX_CHECK_COMPILE_FLAG([-O0], [[DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -O0"]],,[[$CXXFLAG_WERROR]])

  # Prefer -g3, fall back to -g if that is unavailable.
  AX_CHECK_COMPILE_FLAG(
    [-g3],
    [[DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -g3"]],
    [AX_CHECK_COMPILE_FLAG([-g],[[DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -g"]],,[[$CXXFLAG_WERROR]])],
    [[$CXXFLAG_WERROR]])

  AX_CHECK_PREPROC_FLAG([-DDEBUG],[[DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DDEBUG"]],,[[$CXXFLAG_WERROR]])
  AX_CHECK_PREPROC_FLAG([-DDEBUG_LOCKORDER],[[DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DDEBUG_LOCKORDER"]],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-ftrapv],[DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -ftrapv"],,[[$CXXFLAG_WERROR]])
else
  # Even with enable_debug=no we build with standard debug symbols
  AX_CHECK_COMPILE_FLAG([-g],[[DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -g"]],,[[$CXXFLAG_WERROR]])

  # -ftrapv and -fwrapv conflict, so we only set this if enable_debug=no
  AX_CHECK_COMPILE_FLAG([-fwrapv],[CXXFLAGS="$CXXFLAGS -fwrapv"],,[[$CXXFLAG_WERROR]])
fi

if test x$use_sanitizers != x; then
  # First check if the compiler accepts flags. If an incompatible pair like
  # -fsanitize=address,thread is used here, this check will fail. This will also
  # fail if a bad argument is passed, e.g. -fsanitize=undfeined
  AX_CHECK_COMPILE_FLAG(
    [[-fsanitize=$use_sanitizers]],
    [[SANITIZER_CXXFLAGS=-fsanitize=$use_sanitizers]],
    [AC_MSG_ERROR([compiler did not accept requested flags])])

  # Some compilers (e.g. GCC) require additional libraries like libasan,
  # libtsan, libubsan, etc. Make sure linking still works with the sanitize
  # flag. This is a separate check so we can give a better error message when
  # the sanitize flags are supported by the compiler but the actual sanitizer
  # libs are missing.
  AX_CHECK_LINK_FLAG(
    [[-fsanitize=$use_sanitizers]],
    [[SANITIZER_LDFLAGS=-fsanitize=$use_sanitizers]],
    [AC_MSG_ERROR([linker did not accept requested flags, you are missing required libraries])])

  AX_CHECK_COMPILE_FLAG(
    [-fno-omit-frame-pointer],
    [SANITIZER_CXXFLAGS="$SANITIZER_CXXFLAGS -fno-omit-frame-pointer"],
    [AC_MSG_ERROR(Cannot enable -fno-omit-frame-pointer)])

  # libc/libstdc++ static linking is not supported for TSan
  case $use_sanitizers in
    *thread*)
      ;;
    *)
      AX_CHECK_LINK_FLAG(
        [-static-libstdc++],
        [SANITIZER_LDFLAGS="$SANITIZER_LDFLAGS -static-libstdc++"],
        [AC_MSG_ERROR(Cannot statically link -static-libstdc++)])
      ;;
  esac

  case $use_sanitizers in
    *address*)
      AX_CHECK_LINK_FLAG(
        [-static-libasan],
        [SANITIZER_LDFLAGS="$SANITIZER_LDFLAGS -static-libasan"],
        [AC_MSG_ERROR(Cannot statically link -static-libasan)])
      ;;
    *)
      ;;
  esac
fi

ERROR_CXXFLAGS=
if test "x$enable_werror" = "xyes"; then
  if test "x$CXXFLAG_WERROR" = "x"; then
    AC_MSG_ERROR("enable-werror set but -Werror is not usable")
  fi
  ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror"
fi


# This enables all warnings and selectively disables some. Individual warnings should be
# 1. fixed,
# 2. locally silenced with an explanatory comment, or
# 3. globally silenced here with an explanatory comment.
#
# Justifications
# • `-Wno-aix-compat`: we don’t want to maintain compatibility with IBM AIX compilers
# • `-Wno-c++-compat`: this warns about an ABI layout incompatibility between C and C++,
#    but we don’t need such compatibility except where we use `extern "C"`
# • `-W*c++*-compat-pedantic`: disables warnings about compatibility with older versions of C++ and
#    enables warnings about compatibility with newer versions of C++ (we need the explicit enabling,
#    even with `-Weverything`, because the earlier disabling of older versions removes some warnings
#    that we do want)
DESIRED_WARNING_CXXFLAGS="\
  --system-header-prefix=boost --system-header-prefix=crypto/ctaes --system-header-prefix=db_cxx \
  --system-header-prefix=event2 --system-header-prefix=gmock --system-header-prefix=gtest \
  --system-header-prefix=leveldb --system-header-prefix=rust --system-header-prefix=sodium \
  --system-header-prefix=univalue --system-header-prefix=zmq \
  -Weverything \
  -Wno-aix-compat -Wno-c++-compat \
  -Wno-c++98-compat-pedantic -Wno-c++11-compat-pedantic -Wno-c++14-compat-pedantic \
  -Wc++17-compat-pedantic -Wc++20-compat-pedantic"

# This list should be incrementally pruned. Each flag should either be enabled or moved to
# `DESIRED_WARNING_CXXFLAGS` with an explanation as to why it remains disabled.
#
# Guidelines for fixing warnings:
# • fix a small number (perhaps one) at a time
# • change `-Wno-foo` to `-Wno-error=foo` locally to get the full report of the warning you enabled
#   (but be careful, as re-running `make` will not recompile files that only warned).
#
# Potential justifications:
# • `-Wno-format-nonliteral`: Bitcoin i18n
# • `-Wno-old-style-cast`: do after `-W*-conversion` to avoid introducing more conversions
# • `-Wno-padded`: is this worth the noise of rearranging fields & adding anonymous bitfields?
# • `-Wno-shadow-field`: conflicts with our style of naming constructor arguments the same as fields
# • `-Wno-shadow-field-in-constructor`: conflicts with our style of naming constructor arguments the
#    same as fields
# • `-Wno-string-conversion`: currently abused to `assert` on strings
# • `-Wno-tautological-type-limit-compare`: I think isn’t portable because of numeric type sizing
DISABLED_WARNING_CXXFLAGS="\
  -Wno-c++20-compat -Wno-cast-align -Wno-cast-qual -Wno-comma -Wno-conditional-uninitialized \
  -Wno-covered-switch-default -Wno-ctad-maybe-unsupported \
  -Wno-delete-non-abstract-non-virtual-dtor -Wno-deprecated-copy -Wno-deprecated-copy-dtor \
  -Wno-deprecated-dynamic-exception-spec -Wno-disabled-macro-expansion -Wno-documentation \
  -Wno-documentation-unknown-command -Wno-double-promotion -Wno-exit-time-destructors \
  -Wno-extra-semi -Wno-extra-semi-stmt -Wno-float-conversion -Wno-float-equal \
  -Wno-format-nonliteral -Wno-global-constructors -Wno-header-hygiene -Wno-ignored-qualifiers \
  -Wno-implicit-fallthrough -Wno-implicit-int-conversion -Wno-implicit-int-float-conversion \
  -Wno-inconsistent-missing-destructor-override -Wno-logical-op-parentheses \
  -Wno-misleading-indentation -Wno-mismatched-tags -Wno-missing-noreturn -Wno-missing-prototypes \
  -Wno-missing-variable-declarations -Wno-newline-eof -Wno-non-virtual-dtor -Wno-old-style-cast \
  -Wno-overloaded-virtual -Wno-padded -Wno-pedantic -Wno-range-loop-construct \
  -Wno-redundant-parens -Wno-reorder-ctor -Wno-reserved-id-macro -Wno-reserved-identifier \
  -Wno-return-type -Wno-shadow -Wno-shadow-field -Wno-shadow-field-in-constructor \
  -Wno-shift-sign-overflow -Wno-shorten-64-to-32 -Wno-sign-compare -Wno-sign-conversion \
  -Wno-string-conversion -Wno-suggest-destructor-override -Wno-suggest-override -Wno-switch-enum \
  -Wno-tautological-type-limit-compare -Wno-tautological-unsigned-enum-zero-compare \
  -Wno-tautological-unsigned-zero-compare -Wno-thread-safety-attributes \
  -Wno-thread-safety-negative -Wno-undef -Wno-undefined-func-template -Wno-uninitialized \
  -Wno-unreachable-code -Wno-unreachable-code-break -Wno-unreachable-code-return \
  -Wno-unused-but-set-variable -Wno-unused-exception-parameter -Wno-unused-function \
  -Wno-unused-macros -Wno-unused-member-function -Wno-unused-parameter -Wno-unused-private-field \
  -Wno-unused-template -Wno-unused-variable -Wno-used-but-marked-unused -Wno-weak-vtables \
  -Wno-zero-as-null-pointer-constant"

# These are sub-flags of those in `DISABLED_WARNING_CXXFLAGS` that allow us to temporarily
# re-enable pieces until the disabling flags are removed.
REENABLED_WARNING_CXXFLAGS="\
  -Wbit-int-extension -Wc++11-extra-semi -Wc++11-long-long -Wc++14-attribute-extensions \
  -Wc++14-binary-literal -Wc++17-attribute-extensions -Wc++20-attribute-extensions \
  -Wc11-extensions -Wcomplex-component-init -Wdelimited-escape-sequence-extension \
  -Wdocumentation-deprecated-sync -Wdocumentation-html -Wembedded-directive -Wempty-init-stmt \
  -Wempty-translation-unit -Wfixed-enum-extension -Wflexible-array-extensions \
  -Wfloat-overflow-conversion -Wfloat-zero-conversion -Wfuture-attribute-extensions \
  -Wgnu-auto-type -Wgnu-binary-literal -Wgnu-case-range -Wgnu-complex-integer \
  -Wgnu-compound-literal-initializer -Wgnu-conditional-omitted-operand -Wgnu-empty-initializer \
  -Wgnu-empty-struct -Wgnu-flexible-array-initializer -Wgnu-flexible-array-union-member \
  -Wgnu-imaginary-constant -Wgnu-include-next -Wgnu-label-as-value -Wgnu-line-marker \
  -Wgnu-null-pointer-arithmetic -Wgnu-pointer-arith -Wgnu-redeclared-enum \
  -Wgnu-statement-expression -Wgnu-union-cast -Wgnu-zero-line-directive \
  -Wignored-reference-qualifiers -Wimplicit-const-int-float-conversion \
  -Wimplicit-fallthrough-per-function -Wimport-preprocessor-directive-pedantic -Winvalid-utf8 \
  -Wkeyword-macro -Wlanguage-extension-token -Wlong-long -Wmicrosoft-charize \
  -Wmicrosoft-comment-paste -Wmicrosoft-cpp-macro -Wmicrosoft-end-of-file -Wmicrosoft-enum-value \
  -Wmicrosoft-fixed-enum -Wmicrosoft-flexible-array -Wmicrosoft-redeclare-static \
  -Wnested-anon-types -Wnullability-extension -Wobjc-signed-char-bool-implicit-int-conversion \
  -Woverlength-strings -Wpre-c++2b-compat -Wretained-language-linkage \
  -Wshadow-field-in-constructor-modified -Wshadow-ivar -Wsometimes-uninitialized \
  -Wstatic-self-init -Wundefined-internal-type -Wunneeded-member-function \
  -Wunreachable-code-fallthrough -Wunreachable-code-generic-assoc -Wunreachable-code-loop-increment"

WARNING_CXXFLAGS="$DESIRED_WARNING_CXXFLAGS $DISABLED_WARNING_CXXFLAGS $REENABLED_WARNING_CXXFLAGS"

enable_sse42=no
enable_sse41=no
enable_avx2=no
enable_shani=no

if test "x$use_asm" = "xyes"; then

# Check for optional instruction set support. Enabling these does _not_ imply that all code will
# be compiled with them, rather that specific objects/libs may use them after checking for runtime
# compatibility.

dnl x86
AX_CHECK_COMPILE_FLAG([-msse4.2],[[SSE42_CXXFLAGS="-msse4.2"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-msse4.1],[[SSE41_CXXFLAGS="-msse4.1"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-mavx -mavx2],[[AVX2_CXXFLAGS="-mavx -mavx2"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-msse4 -msha],[[SHANI_CXXFLAGS="-msse4 -msha"]],,[[$CXXFLAG_WERROR]])

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $SSE42_CXXFLAGS"
AC_MSG_CHECKING(for SSE4.2 intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #if defined(_MSC_VER)
    #include <intrin.h>
    #elif defined(__GNUC__) && defined(__SSE4_2__)
    #include <nmmintrin.h>
    #endif
  ]],[[
    uint64_t l = 0;
    l = _mm_crc32_u8(l, 0);
    l = _mm_crc32_u32(l, 0);
    l = _mm_crc32_u64(l, 0);
    return l;
  ]])],
 [ AC_MSG_RESULT(yes); enable_sse42=yes],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $SSE41_CXXFLAGS"
AC_MSG_CHECKING(for SSE4.1 intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #include <immintrin.h>
  ]],[[
    __m128i l = _mm_set1_epi32(0);
    return _mm_extract_epi32(l, 3);
  ]])],
 [ AC_MSG_RESULT(yes); enable_sse41=yes; AC_DEFINE(ENABLE_SSE41, 1, [Define this symbol to build code that uses SSE4.1 intrinsics]) ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $AVX2_CXXFLAGS"
AC_MSG_CHECKING(for AVX2 intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #include <immintrin.h>
  ]],[[
    __m256i l = _mm256_set1_epi32(0);
    return _mm256_extract_epi32(l, 7);
  ]])],
 [ AC_MSG_RESULT(yes); enable_avx2=yes; AC_DEFINE(ENABLE_AVX2, 1, [Define this symbol to build code that uses AVX2 intrinsics]) ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $SHANI_CXXFLAGS"
AC_MSG_CHECKING(for SHA-NI intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #include <immintrin.h>
  ]],[[
    __m128i i = _mm_set1_epi32(0);
    __m128i j = _mm_set1_epi32(1);
    __m128i k = _mm_set1_epi32(2);
    return _mm_extract_epi32(_mm_sha256rnds2_epu32(i, i, k), 0);
  ]])],
 [ AC_MSG_RESULT(yes); enable_shani=yes; AC_DEFINE(ENABLE_SHANI, 1, [Define this symbol to build code that uses SHA-NI intrinsics]) ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

# ARM
AX_CHECK_COMPILE_FLAG([-march=armv8-a+crc+crypto],[[ARM_CRC_CXXFLAGS="-march=armv8-a+crc+crypto"]],,[[$CXXFLAG_WERROR]])

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $ARM_CRC_CXXFLAGS"
AC_MSG_CHECKING(for ARM CRC32 intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <arm_acle.h>
    #include <arm_neon.h>
  ]],[[
    __crc32cb(0, 0); __crc32ch(0, 0); __crc32cw(0, 0); __crc32cd(0, 0);
    vmull_p64(0, 0);
  ]])],
 [ AC_MSG_RESULT(yes); enable_arm_crc=yes; ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

fi

CPPFLAGS="$CPPFLAGS -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS"

AC_ARG_WITH([utils],
  [AS_HELP_STRING([--with-utils],
  [build zcash-cli zcash-tx (default=yes)])],
  [build_bitcoin_utils=$withval],
  [build_bitcoin_utils=yes])

AC_ARG_WITH([libs],
  [AS_HELP_STRING([--with-libs],
  [build libraries (default=no)])],
  [build_bitcoin_libs=$withval],
  [build_bitcoin_libs=no])

AC_ARG_WITH([daemon],
  [AS_HELP_STRING([--with-daemon],
  [build bitcoind daemon (default=yes)])],
  [build_bitcoind=$withval],
  [build_bitcoind=yes])

use_pkgconfig=yes
case $host in
  *mingw*)

     dnl pkgconfig does more harm than good with MinGW
     use_pkgconfig=no

     TARGET_OS=windows
     AC_CHECK_LIB([kernel32],   [GetModuleFileNameA],,      AC_MSG_ERROR(libkernel32 missing))
     AC_CHECK_LIB([user32],     [main],,                    AC_MSG_ERROR(libuser32 missing))
     AC_CHECK_LIB([gdi32],      [main],,                    AC_MSG_ERROR(libgdi32 missing))
     AC_CHECK_LIB([comdlg32],   [main],,                    AC_MSG_ERROR(libcomdlg32 missing))
     AC_CHECK_LIB([winmm],      [main],,                    AC_MSG_ERROR(libwinmm missing))
     AC_CHECK_LIB([shell32],    [SHGetSpecialFolderPathW],, AC_MSG_ERROR(libshell32 missing))
     AC_CHECK_LIB([comctl32],   [main],,                    AC_MSG_ERROR(libcomctl32 missing))
     AC_CHECK_LIB([ole32],      [CoCreateInstance],,        AC_MSG_ERROR(libole32 missing))
     AC_CHECK_LIB([oleaut32],   [main],,                    AC_MSG_ERROR(liboleaut32 missing))
     AC_CHECK_LIB([uuid],       [main],,                    AC_MSG_ERROR(libuuid missing))
     AC_CHECK_LIB([advapi32],   [CryptAcquireContextW],,    AC_MSG_ERROR(libadvapi32 missing))
     AC_CHECK_LIB([ws2_32],     [WSAStartup],,              AC_MSG_ERROR(libws2_32 missing))
     AC_CHECK_LIB([shlwapi],    [PathRemoveFileSpecW],,     AC_MSG_ERROR(libshlwapi missing))
     AC_CHECK_LIB([iphlpapi],   [GetAdaptersAddresses],,    AC_MSG_ERROR(libiphlpapi missing))
     AC_CHECK_LIB([ntdll],      [main],,                    AC_MSG_ERROR(libntdll missing))
     AC_CHECK_LIB([bcrypt],     [main],,                    AC_MSG_ERROR(libbcrypt missing))

     dnl -static is interpreted by libtool, where it has a different meaning.
     dnl In libtool-speak, it's -all-static.
     AX_CHECK_LINK_FLAG([[-static]],[LIBTOOL_APP_LDFLAGS="$LIBTOOL_APP_LDFLAGS -all-static"])

     AC_PATH_PROG([MAKENSIS], [makensis], none)
     if test x$MAKENSIS = xnone; then
       AC_MSG_WARN("makensis not found. Cannot create installer.")
     fi

     ZC_REQUIRE_TOOL(WINDRES, windres)

     CPPFLAGS="$CPPFLAGS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB -D_WIN32_WINNT=0x0601 -D_WIN32_IE=0x0501 -DWIN32_LEAN_AND_MEAN"

     dnl libtool insists upon adding -nostdlib and a list of objects/libs to link against.
     dnl That breaks our ability to build dll's with static libgcc/libstdc++/libssp. Override
     dnl its command here, with the predeps/postdeps removed, and -static inserted. Postdeps are
     dnl also overridden to prevent their insertion later.
     dnl This should only affect dll's.
     archive_cmds_CXX="\$CC -shared \$libobjs \$deplibs \$compiler_flags -static -o \$output_objdir/\$soname \${wl}--enable-auto-image-base -Xlinker --out-implib -Xlinker \$lib"
     postdeps_CXX=

     dnl We require Windows 7 (NT 6.1) or later
     AX_CHECK_LINK_FLAG([[-Wl,--major-subsystem-version -Wl,6 -Wl,--minor-subsystem-version -Wl,1]],[LDFLAGS="$LDFLAGS -Wl,--major-subsystem-version -Wl,6 -Wl,--minor-subsystem-version -Wl,1"],,[[$LDFLAG_WERROR]])
     ;;
  *darwin*)
     TARGET_OS=darwin
     if  test x$cross_compiling != xyes; then
       BUILD_OS=darwin
       AC_CHECK_PROG([PORT],port, port)
       if test x$PORT = xport; then
         dnl add default macports paths
         CPPFLAGS="$CPPFLAGS -isystem /opt/local/include"
         LIBS="$LIBS -L/opt/local/lib"
         if test -d /opt/local/include/db62; then
           CPPFLAGS="$CPPFLAGS -I/opt/local/include/db62"
           LIBS="$LIBS -L/opt/local/lib/db62"
         fi
       fi

       AC_CHECK_PROG([BREW],brew, brew)
       if test x$BREW = xbrew; then
         dnl These Homebrew packages may be keg-only, meaning that they won't be found
         dnl in expected paths because they may conflict with system files. Ask
         dnl Homebrew where each one is located, then adjust paths accordingly.
         dnl It's safe to add these paths even if the functionality is disabled by
         dnl the user (--without-wallet for example).

         bdb_prefix=`$BREW --prefix berkeley-db4 2>/dev/null`
         if test x$bdb_prefix != x; then
           CPPFLAGS="$CPPFLAGS -I$bdb_prefix/include"
           LIBS="$LIBS -L$bdb_prefix/lib"
         fi
       fi
     else
       case $build_os in
         *darwin*)
           BUILD_OS=darwin
           ;;
         *)
           AC_PATH_TOOL([INSTALLNAMETOOL], [install_name_tool], install_name_tool)
           AC_PATH_TOOL([OTOOL], [otool], otool)
           AC_PATH_PROGS([GENISOIMAGE], [genisoimage mkisofs],genisoimage)

           dnl libtool will try to strip the static lib, which is a problem for
           dnl cross-builds because strip attempts to call a hard-coded ld,
           dnl which may not exist in the path. Stripping the .a is not
           dnl necessary, so just disable it.
           old_striplib=
           ;;
       esac
     fi

     AX_CHECK_LINK_FLAG([[-Wl,-headerpad_max_install_names]], [LDFLAGS="$LDFLAGS -Wl,-headerpad_max_install_names"])
     CPPFLAGS="$CPPFLAGS -DMAC_OSX"
     OBJCXXFLAGS="$CXXFLAGS"
     ;;
   *android*)
     dnl make sure android stays above linux for hosts like *linux-android*
     TARGET_OS=android
     ;;
   *linux*)
     TARGET_OS=linux
     ;;
esac

if test x$use_pkgconfig = xyes; then
  m4_ifndef([PKG_PROG_PKG_CONFIG], [AC_MSG_ERROR(PKG_PROG_PKG_CONFIG macro not found. Please install pkg-config and re-run autogen.sh.)])
  m4_ifdef([PKG_PROG_PKG_CONFIG], [
  PKG_PROG_PKG_CONFIG
  if test x"$PKG_CONFIG" = "x"; then
    AC_MSG_ERROR(pkg-config not found.)
  fi
  ])
fi

if test x$use_lcov = xyes; then
  ZC_REQUIRE_TOOL(GCOV, gcov)
  ZC_REQUIRE_PROG(LCOV, lcov)
  ZC_REQUIRE_PROG(GENHTML, genhtml)

  LCOV="$LCOV --gcov-tool=$GCOV --rc lcov_branch_coverage=1"
  GENHTML="$GENHTML --branch-coverage"
  AX_CHECK_COMPILE_FLAG([--coverage],[CXXFLAGS="$CXXFLAGS --coverage"],
    [AC_MSG_ERROR("lcov testing requested but --coverage flag does not work")])
fi

dnl Check for endianness
AC_C_BIGENDIAN

dnl Check for pthread compile/link requirements
AX_PTHREAD

# The following macro will add the necessary defines to bitcoin-config.h, but
# they also need to be passed down to any subprojects. Pull the results out of
# the cache and add them to CPPFLAGS.
AC_SYS_LARGEFILE
# detect POSIX or GNU variant of strerror_r
AC_FUNC_STRERROR_R

if test x$ac_cv_sys_file_offset_bits != x &&
   test x$ac_cv_sys_file_offset_bits != xno &&
   test x$ac_cv_sys_file_offset_bits != xunknown; then
  CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits"
fi

if test x$ac_cv_sys_large_files != x &&
   test x$ac_cv_sys_large_files != xno &&
   test x$ac_cv_sys_large_files != xunknown; then
  CPPFLAGS="$CPPFLAGS -D_LARGE_FILES=$ac_cv_sys_large_files"
fi

AX_GCC_FUNC_ATTRIBUTE([visibility])
AX_GCC_FUNC_ATTRIBUTE([dllexport])
AX_GCC_FUNC_ATTRIBUTE([dllimport])

if test x$use_glibc_compat != xno; then

  #glibc absorbed clock_gettime in 2.17. librt (its previous location) is safe to link
  #in anyway for back-compat.
  AC_CHECK_LIB([rt],[clock_gettime],, AC_MSG_ERROR(lib missing))

  #__fdelt_chk's params and return type have changed from long unsigned int to long int.
  # See which one is present here.
  AC_MSG_CHECKING(__fdelt_chk type)
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#ifdef _FORTIFY_SOURCE
                    #undef _FORTIFY_SOURCE
                  #endif
                  #define _FORTIFY_SOURCE 2
                  #include <sys/select.h>
     extern "C" long unsigned int __fdelt_warn(long unsigned int);]],[[]])],
    [ fdelt_type="long unsigned int"],
    [ fdelt_type="long int"])
  AC_MSG_RESULT($fdelt_type)
  AC_DEFINE_UNQUOTED(FDELT_TYPE, $fdelt_type,[parameter and return value type for __fdelt_chk])
else
  AC_SEARCH_LIBS([clock_gettime],[rt])
fi

if test "x$enable_gprof" = xyes; then
    dnl -pg is incompatible with -pie. Since hardening and profiling together doesn't make sense,
    dnl we simply make them mutually exclusive here. Additionally, hardened toolchains may force
    dnl -pie by default, in which case it needs to be turned off with -no-pie.

    if test x$use_hardening = xyes; then
        AC_MSG_ERROR(gprof profiling is not compatible with hardening. Reconfigure with --disable-hardening or --disable-gprof)
    fi
    use_hardening=no
    AX_CHECK_COMPILE_FLAG([-pg],[GPROF_CXXFLAGS="-pg"],
        [AC_MSG_ERROR(gprof profiling requested but not available)], [[$CXXFLAG_WERROR]])

    AX_CHECK_LINK_FLAG([[-no-pie]], [GPROF_LDFLAGS="-no-pie"])
    AX_CHECK_LINK_FLAG([[-pg]],[GPROF_LDFLAGS="$GPROF_LDFLAGS -pg"],
        [AC_MSG_ERROR(gprof profiling requested but not available)], [[$GPROF_LDFLAGS]])
fi

if test x$TARGET_OS != xwindows; then
  # All windows code is PIC, forcing it on just adds useless compile warnings
  AX_CHECK_COMPILE_FLAG([-fPIC],[PIC_FLAGS="-fPIC"])
fi

if test x$use_fuzz_main == xyes; then
    CFLAGS="$CFLAGS -DZCASH_FUZZ=1"
    CXXFLAGS="$CXXFLAGS -DZCASH_FUZZ=1"
fi

if test x$use_hardening != xno; then
  use_hardening=yes
  AX_CHECK_COMPILE_FLAG([-Wformat],[HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -Wformat"],[AC_MSG_ERROR(Cannot enable -Wformat)])
  AX_CHECK_COMPILE_FLAG([-Wformat-security],[HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -Wformat-security"],[AC_MSG_ERROR(Cannot enable -Wformat-security)],[-Wformat])
  AX_CHECK_COMPILE_FLAG([-Wstack-protector],[HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -Wstack-protector"],[AC_MSG_ERROR(Cannot enable -Wstack-protector)])
  AX_CHECK_COMPILE_FLAG([-fstack-protector-all],[HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -fstack-protector-all"],[AC_MSG_ERROR(Cannot enable -fstack-protector-all)])
  AX_CHECK_COMPILE_FLAG([-Wthread-safety-analysis],[HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -Wthread-safety-analysis"],[AC_MSG_ERROR(Cannot enable -Wthread-safety-analysis)])

  AX_CHECK_PREPROC_FLAG([-D_FORTIFY_SOURCE=2],[
    AX_CHECK_PREPROC_FLAG([-U_FORTIFY_SOURCE],[
      HARDENED_CPPFLAGS="$HARDENED_CPPFLAGS -U_FORTIFY_SOURCE"
    ],[AC_MSG_ERROR(Cannot enable -U_FORTIFY_SOURCE)])
    HARDENED_CPPFLAGS="$HARDENED_CPPFLAGS -D_FORTIFY_SOURCE=2"
  ],[AC_MSG_ERROR(Cannot enable -D_FORTIFY_SOURCE=2)])

  if test x$TARGET_OS = xdarwin || test x$TARGET_OS = xwindows; then
    # Xcode's ld (at least ld64-302.3) doesn't support -z
    # mingw-w64's ld (at least mingw-w64 4.0.4-2) also appears to not support -z
    AX_CHECK_LINK_FLAG([[-Wl,-z,relro]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-z,relro"],[AC_MSG_WARN(Cannot enable RELRO)])
    AX_CHECK_LINK_FLAG([[-Wl,-z,now]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-z,now"],[AC_MSG_WARN(Cannot enable BIND_NOW)])
  else
    AX_CHECK_LINK_FLAG([[-Wl,-z,relro]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-z,relro"],[AC_MSG_ERROR(Cannot enable RELRO)])
    AX_CHECK_LINK_FLAG([[-Wl,-z,now]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-z,now"],[AC_MSG_ERROR(Cannot enable BIND_NOW)])
  fi

  if test x$TARGET_OS != xwindows; then
    # All windows code is PIC, forcing it on just adds useless compile warnings
    AX_CHECK_COMPILE_FLAG([-fPIE],[PIE_FLAGS="-fPIE"],[AC_MSG_ERROR(Cannot enable -fPIE)])
    if test x$BUILD_OS = xdarwin; then
      AX_CHECK_LINK_FLAG([[-Wl,-pie]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-pie"],[AC_MSG_ERROR(Cannot enable -Wl,-pie)])
      # Justifications:
      # • `-Wno-return-std-move-in-c++11`: This causes failure in CI if treated as an error,
      #    despite not being documented to exist in any version of Clang (it does not exist on the
      #    Clang used by our other builds).
      # • `-Wno-unknown-warning-option`: The macOS build uses Xcode’s clang, which is relatively old
      #    and doesn’t support all the warnings that other platforms do, so we ignore complaints
      #    about unsupported warnings.
      WARNING_CXXFLAGS="$WARNING_CXXFLAGS -Wno-return-std-move-in-c++11 -Wno-unknown-warning-option"
    else
      AX_CHECK_LINK_FLAG([[-pie]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -pie"],[AC_MSG_ERROR(Cannot enable -pie)])
    fi
  else
    # These are only available on Windows.
    AX_CHECK_LINK_FLAG([[-Wl,--dynamicbase]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--dynamicbase"],[AC_MSG_ERROR(Cannot enable --dynamicbase)])
    AX_CHECK_LINK_FLAG([[-Wl,--nxcompat]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--nxcompat"],[AC_MSG_ERROR(Cannot enable --nxcompat)])
    AX_CHECK_LINK_FLAG([[-Wl,--high-entropy-va]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--high-entropy-va"],[AC_MSG_ERROR(Cannot enable ASLR)])
  fi

  case $host in
    *mingw*)
       AC_CHECK_LIB([ssp],      [main],, AC_MSG_ERROR(lib missing))
    ;;
  esac
fi

dnl this flag screws up non-darwin gcc even when the check fails. special-case it.
if test x$TARGET_OS = xdarwin; then
  AX_CHECK_LINK_FLAG([[-Wl,-dead_strip]], [LDFLAGS="$LDFLAGS -Wl,-dead_strip"])
fi

AC_CHECK_HEADERS([endian.h sys/endian.h byteswap.h stdio.h stdlib.h unistd.h strings.h sys/types.h sys/stat.h sys/select.h sys/prctl.h])
AC_SEARCH_LIBS([getaddrinfo_a], [anl], [AC_DEFINE(HAVE_GETADDRINFO_A, 1, [Define this symbol if you have getaddrinfo_a])])
AC_SEARCH_LIBS([inet_pton], [nsl resolv], [AC_DEFINE(HAVE_INET_PTON, 1, [Define this symbol if you have inet_pton])])

AC_CHECK_DECLS([strnlen])

AC_CHECK_DECLS([le16toh, le32toh, le64toh, htole16, htole32, htole64, be16toh, be32toh, be64toh, htobe16, htobe32, htobe64],,,
		[#if HAVE_ENDIAN_H
                 #include <endian.h>
                 #elif HAVE_SYS_ENDIAN_H
                 #include <sys/endian.h>
                 #endif])

AC_CHECK_DECLS([bswap_16, bswap_32, bswap_64],,,
		[#if HAVE_BYTESWAP_H
                 #include <byteswap.h>
                 #endif])

AC_CHECK_DECLS([__builtin_clz, __builtin_clzl, __builtin_clzll])

dnl Check for MSG_NOSIGNAL
AC_MSG_CHECKING(for MSG_NOSIGNAL)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/socket.h>]],
 [[ int f = MSG_NOSIGNAL; ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_MSG_NOSIGNAL, 1,[Define this symbol if you have MSG_NOSIGNAL]) ],
 [ AC_MSG_RESULT(no)]
)

dnl check for gmtime_r(), fallback to gmtime_s() if that is unavailable
dnl fail if neither are available.
AC_MSG_CHECKING(for gmtime_r)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <ctime>]],
  [[ gmtime_r((const time_t *) nullptr, (struct tm *) nullptr); ]])],
  [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_GMTIME_R, 1, [Define this symbol if gmtime_r is available]) ],
  [ AC_MSG_RESULT(no);
    AC_MSG_CHECKING(for gmtime_s);
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <ctime>]],
       [[ gmtime_s((struct tm *) nullptr, (const time_t *) nullptr); ]])],
       [ AC_MSG_RESULT(yes)],
       [ AC_MSG_RESULT(no); AC_MSG_ERROR(Both gmtime_r and gmtime_s are unavailable) ]
    )
  ]
)

AC_MSG_CHECKING([for visibility attribute])
AC_LINK_IFELSE([AC_LANG_SOURCE([
  int foo_def( void ) __attribute__((visibility("default")));
  int main(){}
  ])],
  [
    AC_DEFINE(HAVE_VISIBILITY_ATTRIBUTE,1,[Define if the visibility attribute is supported.])
    AC_MSG_RESULT(yes)
  ],
  [
    AC_MSG_RESULT(no)
    if test x$use_reduce_exports = xyes; then
      AC_MSG_ERROR([Cannot find a working visibility attribute. Use --disable-reduce-exports.])
    fi
  ]
)

TEMP_LDFLAGS="$LDFLAGS"
LDFLAGS="$TEMP_LDFLAGS $PTHREAD_CFLAGS"
AC_MSG_CHECKING([for thread_local support])
AC_LINK_IFELSE([AC_LANG_SOURCE([
  #include <thread>
  static thread_local int foo = 0;
  static void run_thread() { foo++;}
  int main(){
  for(int i = 0; i < 10; i++) { std::thread(run_thread).detach();}
  return foo;
  }
  ])],
  [
    AC_DEFINE(HAVE_THREAD_LOCAL,1,[Define if thread_local is supported.])
    AC_MSG_RESULT(yes)
  ],
  [
    AC_MSG_RESULT(no)
  ]
)
LDFLAGS="$TEMP_LDFLAGS"

dnl LevelDB platform checks
AC_MSG_CHECKING(for fdatasync)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <unistd.h>]],
 [[ fdatasync(0); ]])],
 [ AC_MSG_RESULT(yes); HAVE_FDATASYNC=1 ],
 [ AC_MSG_RESULT(no); HAVE_FDATASYNC=0 ]
)

AC_MSG_CHECKING(for F_FULLFSYNC)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <fcntl.h>]],
 [[ fcntl(0, F_FULLFSYNC, 0); ]])],
 [ AC_MSG_RESULT(yes); HAVE_FULLFSYNC=1 ],
 [ AC_MSG_RESULT(no); HAVE_FULLFSYNC=0 ]
)

AC_MSG_CHECKING(for O_CLOEXEC)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <fcntl.h>]],
 [[ open("", O_CLOEXEC); ]])],
 [ AC_MSG_RESULT(yes); HAVE_O_CLOEXEC=1 ],
 [ AC_MSG_RESULT(no); HAVE_O_CLOEXEC=0 ]
)

dnl crc32c platform checks
AC_MSG_CHECKING(for __builtin_prefetch)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ ]], [[
  char data = 0;
  const char* address = &data;
  __builtin_prefetch(address, 0, 0);
  ]])],
 [ AC_MSG_RESULT(yes); HAVE_BUILTIN_PREFETCH=1 ],
 [ AC_MSG_RESULT(no); HAVE_BUILTIN_PREFETCH=0 ]
)

AC_MSG_CHECKING(for _mm_prefetch)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <xmmintrin.h>]], [[
  char data = 0;
  const char* address = &data;
  _mm_prefetch(address, _MM_HINT_NTA);
  ]])],
 [ AC_MSG_RESULT(yes); HAVE_MM_PREFETCH=1 ],
 [ AC_MSG_RESULT(no); HAVE_MM_PREFETCH=0 ]
)

AC_MSG_CHECKING(for strong getauxval support in the system headers)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <arm_acle.h>
    #include <arm_neon.h>
    #include <sys/auxv.h>
  ]], [[
    getauxval(AT_HWCAP);
  ]])],
 [ AC_MSG_RESULT(yes); HAVE_STRONG_GETAUXVAL=1 ],
 [ AC_MSG_RESULT(no); HAVE_STRONG_GETAUXVAL=0 ]
)

AC_MSG_CHECKING(for weak getauxval support in the compiler)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    unsigned long getauxval(unsigned long type) __attribute__((weak));
    #define AT_HWCAP 16
  ]], [[
    getauxval(AT_HWCAP);
  ]])],
 [ AC_MSG_RESULT(yes); HAVE_WEAK_GETAUXVAL=1 ],
 [ AC_MSG_RESULT(no); HAVE_WEAK_GETAUXVAL=0 ]
)

dnl Check for reduced exports
if test x$use_reduce_exports = xyes; then
  AX_CHECK_COMPILE_FLAG([-fvisibility=hidden],[RE_CXXFLAGS="-fvisibility=hidden"],
  [AC_MSG_ERROR([Cannot set default symbol visibility. Use --disable-reduce-exports.])])
fi

LEVELDB_CPPFLAGS=
LIBLEVELDB=
LIBMEMENV=
AM_CONDITIONAL([EMBEDDED_LEVELDB],[true])
AC_SUBST(LEVELDB_CPPFLAGS)
AC_SUBST(LIBLEVELDB)
AC_SUBST(LIBMEMENV)

if test x$enable_wallet != xno; then
    dnl Check for libdb_cxx only if wallet enabled
    BITCOIN_FIND_BDB62
fi

if test x$build_bitcoin_utils$build_bitcoind$use_tests = xnonono; then
    use_boost=no
else
    use_boost=yes
fi

if test x$use_boost = xyes; then

dnl Check for boost libs
dnl We need Boost >= 1.62 to fix a potential security bug (https://github.com/zcash/zcash/issues/1241)
dnl We need Boost >= 1.77 to fix usage of statx on Docker (https://github.com/zcash/zcash/issues/4945)
AX_BOOST_BASE([1.77])
AX_BOOST_SYSTEM
AX_BOOST_FILESYSTEM
AX_BOOST_PROGRAM_OPTIONS
AX_BOOST_THREAD
AX_BOOST_CHRONO

fi

if test x$use_reduce_exports = xyes; then
    CXXFLAGS="$CXXFLAGS $RE_CXXFLAGS"
    AX_CHECK_LINK_FLAG([[-Wl,--exclude-libs,ALL]], [RELDFLAGS="-Wl,--exclude-libs,ALL"])
fi

if test x$use_tests = xyes; then

  if test x$HEXDUMP = x; then
    AC_MSG_ERROR(hexdump is required for tests)
  fi


  if test x$use_boost = xyes; then

  AX_BOOST_UNIT_TEST_FRAMEWORK

  dnl Determine if -DBOOST_TEST_DYN_LINK is needed
  AC_MSG_CHECKING([for dynamic linked boost test])
  TEMP_LIBS="$LIBS"
  LIBS="$LIBS $BOOST_LDFLAGS $BOOST_UNIT_TEST_FRAMEWORK_LIB"
  TEMP_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
  AC_LINK_IFELSE([AC_LANG_SOURCE([
       #define BOOST_TEST_DYN_LINK
       #define BOOST_TEST_MAIN
        #include <boost/test/unit_test.hpp>

       ])],
    [AC_MSG_RESULT(yes)]
    [TESTDEFS="$TESTDEFS -DBOOST_TEST_DYN_LINK"],
    [AC_MSG_RESULT(no)])
  LIBS="$TEMP_LIBS"
  CPPFLAGS="$TEMP_CPPFLAGS"

  fi
fi

if test x$use_boost = xyes; then
BOOST_LIBS="$BOOST_LDFLAGS $BOOST_SYSTEM_LIB $BOOST_FILESYSTEM_LIB $BOOST_PROGRAM_OPTIONS_LIB $BOOST_THREAD_LIB $BOOST_CHRONO_LIB"
fi

if test x$use_pkgconfig = xyes; then
  : dnl
  m4_ifdef(
    [PKG_CHECK_MODULES],
    [
      if test x$build_bitcoin_utils$build_bitcoind$bitcoin_enable_qt$use_tests != xnononono; then
        PKG_CHECK_MODULES([EVENT], [libevent],, [AC_MSG_ERROR(libevent not found.)])
        if test x$TARGET_OS != xwindows; then
          PKG_CHECK_MODULES([EVENT_PTHREADS], [libevent_pthreads],, [AC_MSG_ERROR(libevent_pthreads not found.)])
        fi
      fi

      if test "x$use_zmq" = "xyes"; then
        PKG_CHECK_MODULES([ZMQ],[libzmq >= 4],
          [AC_DEFINE([ENABLE_ZMQ],[1],[Define to 1 to enable ZMQ functions])],
          [AC_DEFINE([ENABLE_ZMQ],[0],[Define to 1 to enable ZMQ functions])
           AC_MSG_WARN([libzmq version 4.x or greater not found, disabling])
           use_zmq=no])
      else
          AC_DEFINE_UNQUOTED([ENABLE_ZMQ],[0],[Define to 1 to enable ZMQ functions])
      fi
    ]
  )
else

  if test x$build_bitcoin_utils$build_bitcoind$bitcoin_enable_qt$use_tests != xnononono; then
    AC_CHECK_HEADER([event2/event.h],, AC_MSG_ERROR(libevent headers missing),)
    AC_CHECK_LIB([event],[main],EVENT_LIBS=-levent,AC_MSG_ERROR(libevent missing))
    if test x$TARGET_OS != xwindows; then
      AC_CHECK_LIB([event_pthreads],[main],EVENT_PTHREADS_LIBS=-levent_pthreads,AC_MSG_ERROR(libevent_pthreads missing))
    fi
  fi

  if test "x$use_zmq" = "xyes"; then
     AC_CHECK_HEADER([zmq.h],
       [AC_DEFINE([ENABLE_ZMQ],[1],[Define to 1 to enable ZMQ functions])],
       [AC_MSG_WARN([zmq.h not found, disabling zmq support])
        use_zmq=no
        AC_DEFINE([ENABLE_ZMQ],[0],[Define to 1 to enable ZMQ functions])])
     AC_CHECK_LIB([zmq],[zmq_ctx_shutdown],ZMQ_LIBS=-lzmq,
       [AC_MSG_WARN([libzmq >= 4.0 not found, disabling zmq support])
        use_zmq=no
        AC_DEFINE([ENABLE_ZMQ],[0],[Define to 1 to enable ZMQ functions])])
  else
    AC_DEFINE_UNQUOTED([ENABLE_ZMQ],[0],[Define to 1 to enable ZMQ functions])
  fi

  if test "x$use_zmq" = "xyes"; then
    dnl Assume libzmq was built for static linking
    case $host in
      *mingw*)
        ZMQ_CFLAGS="$ZMQ_CFLAGS -DZMQ_STATIC"
      ;;
    esac
  fi
fi

RUST_LIBS=""
case $host in
  *mingw*)
    RUST_LIBS="$RUST_LIBS -luserenv"
    ;;
  *)
    RUST_LIBS="$RUST_LIBS -ldl"
    ;;
esac

# Additional Zcash flags
AX_CHECK_COMPILE_FLAG([-fno-strict-aliasing],[CXXFLAGS="$CXXFLAGS -fno-strict-aliasing"])
AX_CHECK_COMPILE_FLAG([-Wno-builtin-declaration-mismatch],[CXXFLAGS="$CXXFLAGS -Wno-builtin-declaration-mismatch"],,[[$CXXFLAG_WERROR]])

LIBZCASH_LIBS="$BOOST_SYSTEM_LIB -lsodium $RUST_LIBS"

AC_MSG_CHECKING([whether to build bitcoind])
AM_CONDITIONAL([BUILD_BITCOIND], [test x$build_bitcoind = xyes])
AC_MSG_RESULT($build_bitcoind)

AC_MSG_CHECKING([whether to build utils (zcash-cli zcash-tx)])
AM_CONDITIONAL([BUILD_BITCOIN_UTILS], [test x$build_bitcoin_utils = xyes])
AC_MSG_RESULT($build_bitcoin_utils)

AC_MSG_CHECKING([whether to build libraries])
AM_CONDITIONAL([BUILD_BITCOIN_LIBS], [test x$build_bitcoin_libs = xyes])
if test x$build_bitcoin_libs = xyes; then
  AC_DEFINE(HAVE_SCRIPT_LIB, 1, [Define this symbol if the script lib has been built])
  AC_CONFIG_FILES([libzcash_script.pc:libzcash_script.pc.in])
fi

AC_MSG_RESULT($build_bitcoin_libs)

AC_LANG_POP

if test "x$use_ccache" != "xno"; then
  AC_MSG_CHECKING(if ccache should be used)
  if test x$CCACHE = x; then
    if test "x$use_ccache" = "xyes"; then
      AC_MSG_ERROR([ccache not found.]);
    else
      use_ccache=no
    fi
  else
    use_ccache=yes
    CC="$ac_cv_path_CCACHE $CC"
    CXX="$ac_cv_path_CCACHE $CXX"
  fi
  AC_MSG_RESULT($use_ccache)
fi
if test "x$use_ccache" = "xyes"; then
    AX_CHECK_PREPROC_FLAG([-Qunused-arguments],[CPPFLAGS="-Qunused-arguments $CPPFLAGS"])
fi

dnl enable wallet
AC_MSG_CHECKING([if wallet should be enabled])
if test x$enable_wallet != xno; then
  AC_MSG_RESULT(yes)
  AC_DEFINE_UNQUOTED([ENABLE_WALLET],[1],[Define to 1 to enable wallet functions])

else
  AC_MSG_RESULT(no)
fi

dnl enable mining
AC_MSG_CHECKING([if mining should be enabled])
if test x$enable_mining != xno; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(ENABLE_MINING, 1, [Define to 1 to enable mining functions])

else
  AC_MSG_RESULT(no)
fi

AM_CONDITIONAL([ENABLE_ZMQ], [test "x$use_zmq" = "xyes"])

AC_MSG_CHECKING([whether to build test_bitcoin])
if test x$use_tests = xyes; then
  AC_MSG_RESULT([yes])
  BUILD_TEST="yes"
else
  AC_MSG_RESULT([no])
  BUILD_TEST=""
fi

AC_MSG_CHECKING([whether to reduce exports])
if test x$use_reduce_exports = xyes; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

if test x$build_bitcoin_utils$build_bitcoin_libs$build_bitcoind$use_tests = xnononono; then
  AC_MSG_ERROR([No targets! Please specify at least one of: --with-utils --with-libs --with-daemon or --enable-tests])
fi

AM_CONDITIONAL([TARGET_DARWIN], [test x$TARGET_OS = xdarwin])
AM_CONDITIONAL([BUILD_DARWIN], [test x$BUILD_OS = xdarwin])
AM_CONDITIONAL([TARGET_WINDOWS], [test x$TARGET_OS = xwindows])
AM_CONDITIONAL([ENABLE_WALLET],[test x$enable_wallet = xyes])
AM_CONDITIONAL([ENABLE_MINING],[test x$enable_mining = xyes])
AM_CONDITIONAL([ENABLE_TESTS],[test x$BUILD_TEST = xyes])
AM_CONDITIONAL([ENABLE_BENCH],[test x$use_bench = xyes])
AM_CONDITIONAL([USE_LCOV],[test x$use_lcov = xyes])
AM_CONDITIONAL([GLIBC_BACK_COMPAT],[test x$use_glibc_compat = xyes])
AM_CONDITIONAL([HARDEN],[test x$use_hardening = xyes])
AM_CONDITIONAL([ENABLE_SSE42],[test x$enable_sse42 = xyes])
AM_CONDITIONAL([ENABLE_SSE41],[test x$enable_sse41 = xyes])
AM_CONDITIONAL([ENABLE_AVX2],[test x$enable_avx2 = xyes])
AM_CONDITIONAL([ENABLE_SHANI],[test x$enable_shani = xyes])
AM_CONDITIONAL([ENABLE_ARM_CRC],[test x$enable_arm_crc = xyes])
AM_CONDITIONAL([USE_ASM],[test x$use_asm = xyes])
AM_CONDITIONAL([WORDS_BIGENDIAN],[test x$ac_cv_c_bigendian = xyes])

AC_DEFINE(CLIENT_VERSION_MAJOR, _CLIENT_VERSION_MAJOR, [Major version])
AC_DEFINE(CLIENT_VERSION_MINOR, _CLIENT_VERSION_MINOR, [Minor version])
AC_DEFINE(CLIENT_VERSION_REVISION, _CLIENT_VERSION_REVISION, [Build revision])
AC_DEFINE(CLIENT_VERSION_BUILD, _CLIENT_VERSION_BUILD, [Version Build])
AC_DEFINE(CLIENT_VERSION_IS_RELEASE, _CLIENT_VERSION_IS_RELEASE, [Version is release])
AC_DEFINE(COPYRIGHT_YEAR, _COPYRIGHT_YEAR, [Version is release])
AC_SUBST(CLIENT_VERSION_MAJOR, _CLIENT_VERSION_MAJOR)
AC_SUBST(CLIENT_VERSION_MINOR, _CLIENT_VERSION_MINOR)
AC_SUBST(CLIENT_VERSION_REVISION, _CLIENT_VERSION_REVISION)
AC_SUBST(CLIENT_VERSION_BUILD, _CLIENT_VERSION_BUILD)
AC_SUBST(CLIENT_VERSION_IS_RELEASE, _CLIENT_VERSION_IS_RELEASE)
AC_SUBST(COPYRIGHT_YEAR, _COPYRIGHT_YEAR)
AC_SUBST(BITCOIN_DAEMON_NAME)
AC_SUBST(BITCOIN_CLI_NAME)
AC_SUBST(BITCOIN_TX_NAME)

AC_SUBST(RUST_TARGET)
AC_SUBST(RUST_VENDORED_SOURCES)
AC_SUBST(RELDFLAGS)
AC_SUBST(DEBUG_CPPFLAGS)
AC_SUBST(DEBUG_CXXFLAGS)
AC_SUBST(WARNING_CXXFLAGS)
AC_SUBST(ERROR_CXXFLAGS)
AC_SUBST(GPROF_CXXFLAGS)
AC_SUBST(GPROF_LDFLAGS)
AC_SUBST(HARDENED_CXXFLAGS)
AC_SUBST(HARDENED_CPPFLAGS)
AC_SUBST(HARDENED_LDFLAGS)
AC_SUBST(PIC_FLAGS)
AC_SUBST(PIE_FLAGS)
AC_SUBST(SANITIZER_CXXFLAGS)
AC_SUBST(SANITIZER_LDFLAGS)
AC_SUBST(SSE42_CXXFLAGS)
AC_SUBST(SSE41_CXXFLAGS)
AC_SUBST(AVX2_CXXFLAGS)
AC_SUBST(SHANI_CXXFLAGS)
AC_SUBST(ARM_CRC_CXXFLAGS)
AC_SUBST(LIBTOOL_APP_LDFLAGS)
AC_SUBST(BOOST_LIBS)
AC_SUBST(TESTDEFS)
AC_SUBST(EVENT_LIBS)
AC_SUBST(EVENT_PTHREADS_LIBS)
AC_SUBST(ZMQ_LIBS)
AC_SUBST(HAVE_GMTIME_R)
AC_SUBST(LIBZCASH_LIBS)
AC_SUBST(HAVE_FDATASYNC)
AC_SUBST(HAVE_FULLFSYNC)
AC_SUBST(HAVE_O_CLOEXEC)
AC_SUBST(HAVE_BUILTIN_PREFETCH)
AC_SUBST(HAVE_MM_PREFETCH)
AC_SUBST(HAVE_STRONG_GETAUXVAL)
AC_SUBST(HAVE_WEAK_GETAUXVAL)
AC_CONFIG_FILES([Makefile src/Makefile doc/man/Makefile src/test/buildenv.py])
AC_CONFIG_FILES([qa/pull-tester/tests_config.ini],[chmod +x qa/pull-tester/tests_config.ini])
AC_CONFIG_LINKS([qa/pull-tester/rpc-tests.py:qa/pull-tester/rpc-tests.py])

dnl boost's m4 checks do something really nasty: they export these vars. As a
dnl result, they leak into secp256k1's configure and crazy things happen.
dnl Until this is fixed upstream and we've synced, we'll just un-export them.
CPPFLAGS_TEMP="$CPPFLAGS"
unset CPPFLAGS
CPPFLAGS="$CPPFLAGS_TEMP"

LDFLAGS_TEMP="$LDFLAGS"
unset LDFLAGS
LDFLAGS="$LDFLAGS_TEMP"

LIBS_TEMP="$LIBS"
unset LIBS
LIBS="$LIBS_TEMP"

ac_configure_args="${ac_configure_args} --disable-shared --with-pic --enable-benchmark=no --enable-module-recovery"
AC_CONFIG_SUBDIRS([src/secp256k1 src/univalue])

AC_OUTPUT

dnl Taken from https://wiki.debian.org/RpathIssue
case $host in
   *-*-linux-gnu)
     AC_MSG_RESULT([Fixing libtool for -rpath problems.])
     sed < libtool > libtool-2 \
     's/^hardcode_libdir_flag_spec.*$'/'hardcode_libdir_flag_spec=" -D__LIBTOOL_IS_A_FOOL__ "/'
     mv libtool-2 libtool
     chmod 755 libtool
   ;;
esac

dnl Replace the BUILDDIR path with the correct Windows path if compiling on Native Windows
case ${OS} in
   *Windows*)
     sed  's/BUILDDIR="\/\([[a-z]]\)/BUILDDIR="\1:/'  qa/pull-tester/tests_config.py > qa/pull-tester/tests_config-2.py
     mv qa/pull-tester/tests_config-2.py qa/pull-tester/tests_config.py
     chmod +x qa/pull-tester/tests_config.py
   ;;
esac

echo 
echo "Options used to compile and link:"
echo "  with wallet   = $enable_wallet"
echo "  with zmq      = $use_zmq"
echo "  with test     = $use_tests"
echo "  use asm       = $use_asm"
echo "  sanitizers    = $use_sanitizers"
echo "  debug enabled = $enable_debug"
echo "  gprof enabled = $enable_gprof"
echo "  werror        = $enable_werror"
echo 
echo "  target os     = $TARGET_OS"
echo "  build os      = $BUILD_OS"
echo
echo "  CC            = $CC"
echo "  CFLAGS        = $CFLAGS"
echo "  CPPFLAGS      = $DEBUG_CPPFLAGS $HARDENED_CPPFLAGS $CPPFLAGS"
echo "  CXX           = $CXX"
echo "  CXXFLAGS      = $DEBUG_CXXFLAGS $HARDENED_CXXFLAGS $ERROR_CXXFLAGS $GPROF_CXXFLAGS $CXXFLAGS"
echo "  LDFLAGS       = $PTHREAD_CFLAGS $HARDENED_LDFLAGS $GPROF_LDFLAGS $LDFLAGS"
echo "  ARFLAGS       = $ARFLAGS"
echo 

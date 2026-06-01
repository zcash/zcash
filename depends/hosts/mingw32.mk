# The Windows cross toolchain is llvm-mingw (see packages/native_llvm_mingw.mk):
# a self-consistent UCRT bundle of Clang, LLD, mingw-w64, libc++, libc++abi,
# libunwind, and winpthreads. Using it instead of the host's system mingw-w64
# means the whole tree shares one CRT (UCRT) and one libc++ ABI.
mingw32_native_toolchain=native_llvm_mingw
mingw32_native_binutils=native_llvm_mingw

# native_llvm_mingw symlinks its target-prefixed tools into $(build_prefix)/bin
# (the native toolchain bin directory), so we reference them by name without a
# path. That directory is on PATH during package builds and is also the prefix the
# generated config.site prepends to each tool name; an absolute path here would be
# doubled by that prefix and break configure. The clang/clang++ wrappers select the
# mingw target, the bundled UCRT sysroot, libc++, and LLD automatically, so no
# -target/--sysroot/-stdlib/-fuse-ld flags are needed.
mingw32_CC=x86_64-w64-mingw32-clang
mingw32_CXX=x86_64-w64-mingw32-clang++
mingw32_AR=x86_64-w64-mingw32-ar
mingw32_RANLIB=x86_64-w64-mingw32-ranlib
mingw32_NM=x86_64-w64-mingw32-nm
mingw32_STRIP=x86_64-w64-mingw32-strip

mingw32_CFLAGS=-pipe
mingw32_CXXFLAGS=$(mingw32_CFLAGS)

# Target Windows 7, matching configure.ac, so the whole tree agrees on the
# minimum Windows version rather than inheriting llvm-mingw's newer default.
# (Boost is held at 1.88.0 for the same reason; see packages/boost.mk.)
mingw32_CPPFLAGS=-D_WIN32_WINNT=0x0601

mingw32_release_CFLAGS=-O3
mingw32_release_CXXFLAGS=$(mingw32_release_CFLAGS)

mingw32_debug_CFLAGS=-O0
mingw32_debug_CXXFLAGS=$(mingw32_debug_CFLAGS)

mingw32_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC

mingw_cmake_system=Windows

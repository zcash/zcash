mingw32_CC=x86_64-w64-mingw32-gcc-posix
mingw32_CXX=x86_64-w64-mingw32-g++-posix
mingw32_CFLAGS=-pipe -std=c11
mingw32_CXXFLAGS=-pipe -std=c++11

mingw32_release_CFLAGS=-O3
mingw32_release_CXXFLAGS=$(mingw32_CXXFLAGS) $(mingw32_release_CFLAGS)

mingw32_debug_CFLAGS=-g -O0
mingw32_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
mingw32_debug_CXXFLAGS=$(mingw32_CXXFLAGS) $(mingw32_debug_CFLAGS)

OSX_MIN_VERSION=10.8
OSX_SDK_VERSION=10.11
OSX_SDK=$(SDK_PATH)/MacOSX$(OSX_SDK_VERSION).sdk
#<<<<<<< HEAD
#LD64_VERSION=241.9
#darwin_CC=gcc-5 -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) --sysroot $(OSX_SDK) -mlinker-version=$(LD64_VERSION)
#darwin_CXX=g++-5 -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) --sysroot $(OSX_SDK) -mlinker-version=$(LD64_VERSION)
#=======
LD64_VERSION=253.9
darwin_CC=clang -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) --sysroot $(OSX_SDK) -mlinker-version=$(LD64_VERSION)
darwin_CXX=clang++ -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) --sysroot $(OSX_SDK) -mlinker-version=$(LD64_VERSION) -stdlib=libc++
#>>>>>>> zcash/master

darwin_CFLAGS=-pipe
darwin_CXXFLAGS=$(darwin_CFLAGS)

darwin_release_CFLAGS=-O2
darwin_release_CXXFLAGS=$(darwin_release_CFLAGS)

darwin_debug_CFLAGS=-O1
darwin_debug_CXXFLAGS=$(darwin_debug_CFLAGS)

darwin_native_toolchain=native_cctools

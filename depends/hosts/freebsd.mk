freebsd_CFLAGS=-fPIC
freebsd_CXXFLAGS=$(freebsd_CFLAGS)

# untested, but <https://lists.freebsd.org/pipermail/freebsd-toolchain/2016-August/002314.html> suggests it should work
ifeq ($(use_hardening),yes)
  linux_LDFLAGS += -Wl,-z,relro -Wl,-z,now -pie
endif

i686_freebsd_CC=$(default_host_CC) -m32
i686_freebsd_CXX=$(default_host_CXX) -m32
x86_64_freebsd_CC=$(default_host_CC) -m64
x86_64_freebsd_CXX=$(default_host_CXX) -m64

freebsd_cmake_system=FreeBSD

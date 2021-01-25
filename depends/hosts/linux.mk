linux_CFLAGS=-pipe
linux_CXXFLAGS=$(linux_CFLAGS)

linux_release_CFLAGS=-O3
linux_release_CXXFLAGS=$(linux_release_CFLAGS)

linux_debug_CFLAGS=-O0
linux_debug_CXXFLAGS=$(linux_debug_CFLAGS)

linux_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC

linux_LDFLAGS?=-fuse-ld=lld

i686_linux_CC=$(default_host_CC) -m32
i686_linux_CXX=$(default_host_CXX) -m32
x86_64_linux_CC=$(default_host_CC) -m64
x86_64_linux_CXX=$(default_host_CXX) -m64

# Clang doesn't appear to find these multilib paths by default,
# so help it out if we are cross-compiling.
ifneq ($(canonical_host),$(build))
  # CFLAGS is copied to CXXFLAGS after it is fully-evaluated.
  linux_CFLAGS += -idirafter /usr/$(host)/include
  linux_LDFLAGS += -L/usr/$(host)/lib
endif

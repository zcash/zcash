i686_linux_CC=$(default_host_CC) -m32
i686_linux_CXX=$(default_host_CXX) -m32
x86_64_linux_CC=$(default_host_CC) -m64
x86_64_linux_CXX=$(default_host_CXX) -m64

# Clang doesn't appear to find these multilib paths by default,
# so help it out if we are cross-compiling.
ifneq ($(canonical_host),$(build))
  i686_linux_CC += -I/usr/$(host)/include
  i686_linux_CXX += -I/usr/$(host)/include
  x86_64_linux_CC += -I/usr/$(host)/include
  x86_64_linux_CXX += -I/usr/$(host)/include
  linux_CC = $(default_host_CC) -I/usr/$(host)/include
  linux_CXX = $(default_host_CXX) -I/usr/$(host)/include
endif

linux_cmake_system=Linux

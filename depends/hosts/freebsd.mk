freebsd_CFLAGS=-pipe
freebsd_CXXFLAGS=$(freebsd_CFLAGS)

freebsd_release_CFLAGS=-O1
freebsd_release_CXXFLAGS=$(freebsd_release_CFLAGS)

freebsd_debug_CFLAGS=-O1
freebsd_debug_CXXFLAGS=$(freebsd_debug_CFLAGS)

freebsd_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC

ifeq (86,$(findstring 86,$(build_arch)))
i686_freebsd_CC=gcc -m32
i686_freebsd_CXX=g++ -m32
i686_freebsd_AR=ar
i686_freebsd_RANLIB=ranlib
i686_freebsd_NM=nm
i686_freebsd_STRIP=strip

x86_64_freebsd_CC=gcc -m64
x86_64_freebsd_CXX=g++ -m64
x86_64_freebsd_AR=ar
x86_64_freebsd_RANLIB=ranlib
x86_64_freebsd_NM=nm
x86_64_freebsd_STRIP=strip
else
i686_freebsd_CC=$(default_host_CC) -m32
i686_freebsd_CXX=$(default_host_CXX) -m32
x86_64_freebsd_CC=$(default_host_CC) -m64
x86_64_freebsd_CXX=$(default_host_CXX) -m64
endif

# Flag explanations:
#
#     -B$(build_prefix)/bin
#
#         Explicitly point to our binaries (e.g. cctools) so that they are
#         ensured to be found and preferred over other possibilities.
#
default_host_CC = clang -target $(host) -B$(build_prefix)/bin
default_host_CXX = clang++ -target $(host) -stdlib=libc++ -B$(build_prefix)/bin
default_host_AR = llvm-ar
default_host_RANLIB = llvm-ranlib
default_host_STRIP = llvm-strip
default_host_LIBTOOL = $(host_toolchain)libtool
default_host_INSTALL_NAME_TOOL = $(host_toolchain)install_name_tool
default_host_OTOOL = $(host_toolchain)otool
default_host_NM = llvm-nm

default_CFLAGS=-pipe -fvisibility=hidden -fvisibility-inlines-hidden
default_CXXFLAGS=$(default_CFLAGS)

default_release_CFLAGS=-O1
default_release_CXXFLAGS=$(default_release_CFLAGS)

default_debug_CFLAGS=-O1
default_debug_CXXFLAGS=$(default_debug_CFLAGS)
default_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC

$(host_os)_native_binutils?=native_clang
$(host_os)_native_toolchain?=native_clang

define add_host_tool_func
$(host_os)_$1?=$$(default_host_$1)
$(host_arch)_$(host_os)_$1?=$$($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1?=$$($(host_os)_$1)
host_$1=$$($(host_arch)_$(host_os)_$1)
endef

define add_host_flags_func
$(host_arch)_$(host_os)_$1 += $(default_$1)
$(host_arch)_$(host_os)_$1 += $(default_$(release_type)_$1)
$(host_arch)_$(host_os)_$1 += $($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1 += $($(host_os)_$(release_type)_$1)
host_$1 = $$($(host_arch)_$(host_os)_$1)
host_$(release_type)_$1 = $$($(host_arch)_$(host_os)_$(release_type)_$1)
endef

$(foreach tool,CC CXX AR RANLIB STRIP NM LIBTOOL OTOOL INSTALL_NAME_TOOL,$(eval $(call add_host_tool_func,$(tool))))
$(foreach flags,CFLAGS CXXFLAGS CPPFLAGS LDFLAGS, $(eval $(call add_host_flags_func,$(flags))))

# Flag explanations:
#
#     -B$(build_prefix)/bin
#
#         Explicitly point to our binaries (e.g. cctools) so that they are
#         ensured to be found and preferred over other possibilities.
#
default_host_CC = $(build_prefix)/bin/clang -target $(host) -B$(build_prefix)/bin
default_host_CXX = $(build_prefix)/bin/clang++ -target $(host) -B$(build_prefix)/bin -stdlib=libc++
default_host_AR = $(build_prefix)/bin/llvm-ar
default_host_RANLIB = $(build_prefix)/bin/llvm-ranlib
default_host_STRIP = $(build_prefix)/bin/llvm-strip
default_host_LIBTOOL = $(host_toolchain)libtool
default_host_INSTALL_NAME_TOOL = $(host_toolchain)install_name_tool
default_host_OTOOL = $(host_toolchain)otool
default_host_NM = $(build_prefix)/bin/llvm-nm

$(host_os)_native_binutils?=native_clang
$(host_os)_native_toolchain?=native_clang

define add_host_tool_func
ifneq ($(filter $(origin $1),undefined default),)
# Do not consider the well-known var $1 if it is undefined or is taking a value
# that is predefined by "make" (e.g. the make variable "CC" has a predefined
# value of "cc")
$(host_os)_$1?=$$(default_host_$1)
$(host_arch)_$(host_os)_$1?=$$($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1?=$$($(host_os)_$1)
else
$(host_os)_$1=$(or $($1),$($(host_os)_$1),$(default_host_$1))
$(host_arch)_$(host_os)_$1=$(or $($1),$($(host_arch)_$(host_os)_$1),$$($(host_os)_$1))
$(host_arch)_$(host_os)_$(release_type)_$1=$(or $($1),$($(host_arch)_$(host_os)_$(release_type)_$1),$$($(host_os)_$1))
endif
host_$1=$$($(host_arch)_$(host_os)_$1)
endef

define add_host_flags_func
$(host_arch)_$(host_os)_$1 += $($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1 += $($(host_os)_$(release_type)_$1)
host_$1 = $$($(host_arch)_$(host_os)_$1)
host_$(release_type)_$1 = $$($(host_arch)_$(host_os)_$(release_type)_$1)
endef

$(foreach tool,CC CXX AR RANLIB STRIP NM LIBTOOL OTOOL INSTALL_NAME_TOOL,$(eval $(call add_host_tool_func,$(tool))))
$(foreach flags,CFLAGS CXXFLAGS CPPFLAGS LDFLAGS, $(eval $(call add_host_flags_func,$(flags))))

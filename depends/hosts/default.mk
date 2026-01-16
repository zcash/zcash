# ==============================================================================
# Darwin (macOS/iOS) Toolchain Setup
# This configuration defines the compiler, linker, and utility tools 
# specifically for building on or for the Darwin operating system.
# It enforces the use of specific, custom-built binaries over system defaults
# using the -B flag and explicit LLVM tools.
# ==============================================================================

# ------------------------------------------------------------------------------
# 1. Base Tool Definitions (Darwin/macOS Specific)
# ------------------------------------------------------------------------------

# Explanation of -B flag:
# -B$(build_prefix)/bin
# Explicitly directs the compiler (clang) to search the specified directory 
# for auxiliary programs (like assemblers and linkers) before searching the standard places.
# This ensures that our custom-built toolchain binaries (e.g., from cctools) are found.

# Compiler (C)
DEFAULT_HOST_CC := clang -target $(host) -B$(build_prefix)/bin

# Compiler (C++): Uses libc++ as the standard library implementation.
DEFAULT_HOST_CXX := clang++ -target $(host) -B$(build_prefix)/bin -stdlib=libc++

# Archiver/Static Linker
DEFAULT_HOST_AR := llvm-ar

# Symbol Table Indexer
DEFAULT_HOST_RANLIB := llvm-ranlib

# Binary Stripper
DEFAULT_HOST_STRIP := llvm-strip

# Dynamic Library Utility (macOS specific)
DEFAULT_HOST_LIBTOOL := $(host_toolchain)libtool

# Dynamic Library Installation Name Tool (macOS specific)
DEFAULT_HOST_INSTALL_NAME_TOOL := $(host_toolchain)install_name_tool

# Object File Inspector (macOS specific)
DEFAULT_HOST_OTOOL := $(host_toolchain)otool

# Name List/Symbol Table Display
DEFAULT_HOST_NM := llvm-nm

# Set toolchain type based on OS
$(host_os)_NATIVE_BINUTILS ?= native_clang
$(host_os)_NATIVE_TOOLCHAIN ?= native_clang


# ------------------------------------------------------------------------------
# 2. Function Definitions
# ------------------------------------------------------------------------------

# Function to define host tools (CC, CXX, AR, etc.) with multi-level fallbacks.
# The hierarchy of preference is:
# 1. User-defined variable ($1)
# 2. Architecture/OS specific variable ($($(host_arch)_$(host_os)_$1))
# 3. OS specific variable ($($(host_os)_$1))
# 4. Default Host variable ($(default_host_$1))
define add_host_tool_func
ifneq ($(filter $(origin $1),undefined default),)
# Case 1: If the variable ($1) is NOT explicitly defined by the user (i.e., it's undefined or takes a make-predefined value).
$(host_os)_$1 := $$(DEFAULT_HOST_$1)
$(host_arch)_$(host_os)_$1 := $$($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1 := $$($(host_os)_$1)
else
# Case 2: If the variable ($1) IS explicitly defined by the user.
$(host_os)_$1 := $$(or $$($1),$$($(host_os)_$1),$$($$($$($$1))_host_$1))
$(host_arch)_$(host_os)_$1 := $$(or $$($1),$$($(host_arch)_$(host_os)_$1),$$($(host_os)_$1))
$(host_arch)_$(host_os)_$(release_type)_$1 := $$(or $$($1),$$($(host_arch)_$(host_os)_$(release_type)_$1),$$($(host_os)_$1))
endif
# Final variable used by the build system
HOST_$1 := $$($(host_arch)_$(host_os)_$1)
endef


# Function to define host compiler flags (CFLAGS, LDFLAGS, etc.)
# Aggregates OS-specific flags and provides type-specific flag variables.
define add_host_flags_func
$(host_arch)_$(host_os)_$1 += $($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1 += $($(host_os)_$(release_type)_$1)
HOST_$1 := $$($(host_arch)_$(host_os)_$1)
HOST_$(release_type)_$1 := $$($(host_arch)_$(host_os)_$(release_type)_$1)
endef


# ------------------------------------------------------------------------------
# 3. Execution (Apply Functions)
# ------------------------------------------------------------------------------

# Execute the tool definition function for all required tools.
$(foreach tool,CC CXX AR RANLIB STRIP NM LIBTOOL OTOOL INSTALL_NAME_TOOL,$(eval $(call add_host_tool_func,$(tool))))

# Execute the flags definition function for all required flag types.
$(foreach flags,CFLAGS CXXFLAGS CPPFLAGS LDFLAGS, $(eval $(call add_host_flags_func,$(flags))))

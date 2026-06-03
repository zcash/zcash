zcash_packages := libsodium rustcxx utfcpp tl_expected
packages := boost libevent zeromq $(zcash_packages) googletest
native_packages := native_clang native_ccache native_cmake native_fmt native_rust native_cxxbridge native_xxhash native_zstd

ifeq ($(build_os),linux)
native_packages += native_libtinfo5
# Provides libxml2.so.2 for the downloaded LLVM ld.lld/lld on hosts that have
# moved to libxml2.so.16 (libxml2 >= 2.14). See native_libxml2.mk.
native_packages += native_libxml2
endif

wallet_packages=bdb

$(host_arch)_$(host_os)_native_packages += native_b2

# The Windows cross toolchain is provided by llvm-mingw rather than the host's
# system mingw-w64; see hosts/mingw32.mk.
mingw32_native_packages += native_llvm_mingw

ifneq ($(build_os),darwin)
darwin_native_packages=native_cctools
endif

# We use a complete SDK for Darwin, which includes libc++.
ifneq ($(host_os),darwin)
packages += libcxx
endif

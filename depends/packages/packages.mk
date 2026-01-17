zcash_packages := libsodium rustcxx utfcpp tl_expected
packages := boost libevent zeromq $(zcash_packages) googletest
native_packages := native_clang native_ccache native_cmake native_fmt native_rust native_cxxbridge native_xxhash native_zstd

ifeq ($(build_os),linux)
native_packages += native_libtinfo5
endif

wallet_packages=bdb

$(host_arch)_$(host_os)_native_packages += native_b2

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_libtapi

darwin_native_packages+= native_clang
endif

# We use a complete SDK for Darwin, which includes libc++.
ifneq ($(host_os),darwin)
packages += libcxx
endif

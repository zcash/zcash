zcash_packages := libsodium utfcpp
packages := boost libevent zeromq $(zcash_packages) googletest
native_packages := native_ccache native_rust

wallet_packages=bdb

ifneq ($(build_os),darwin)
darwin_native_packages=native_cctools
endif

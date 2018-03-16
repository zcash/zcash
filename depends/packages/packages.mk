rust_crates := crate_libc
rust_packages := rust $(rust_crates) librustzcash
proton_packages := proton
zcash_packages := libgmp libsodium
packages := boost openssl libevent zeromq $(zcash_packages) googletest
native_packages := native_ccache

wallet_packages=bdb

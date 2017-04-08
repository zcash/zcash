rust_packages := rust librustzcash
zcash_packages := libsnark libgmp libsodium

ifeq ($(build_os),darwin)
packages := boost openssl libevent zeromq $(zcash_packages)
else
packages := boost openssl libevent zeromq $(zcash_packages) googletest googlemock
endif


native_packages := native_ccache

wallet_packages=bdb

upnp_packages=miniupnpc

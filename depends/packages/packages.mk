zcash_packages := libsnark libgmp libsodium

ifeq ($(build_os),darwin)
packages := boost openssl $(zcash_packages)
else
packages := boost openssl $(zcash_packages) googletest googlemock
endif


native_packages := native_ccache

wallet_packages=bdb

upnp_packages=miniupnpc

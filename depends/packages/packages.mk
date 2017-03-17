zcash_packages := libsnark libgmp libsodium rust librustzcash
packages := boost openssl zeromq $(zcash_packages) googletest googlemock
native_packages := native_ccache

wallet_packages=bdb

upnp_packages=miniupnpc

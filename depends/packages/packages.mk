zcash_packages := libsnark libgmp libsodium
packages := boost openssl $(zcash_packages) googletest googlemock
packages_darwin:=zeromq
packages_linux:=zeromq
native_packages := native_ccache

wallet_packages=bdb

upnp_packages=miniupnpc

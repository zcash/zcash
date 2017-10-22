rust_packages := rust librustzcash
zcash_packages := libsnark libgmp libsodium

ifeq ($(host_os),linux)
	packages := boost openssl libevent zeromq $(zcash_packages) googletest googlemock
else
	packages := boost openssl libevent zeromq $(zcash_packages) googletest googlemock libcurl
endif


native_packages := native_ccache

wallet_packages=bdb

upnp_packages=miniupnpc

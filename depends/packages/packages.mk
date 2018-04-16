rust_packages := rust librustzcash

proton_packages := proton
zcash_packages := libgmp libsodium
ifeq ($(host_os),linux)
	packages := boost openssl libevent zeromq $(zcash_packages) googletest #googlemock
else
	packages := boost openssl libevent zeromq $(zcash_packages) libcurl googletest #googlemock
endif

native_packages := native_ccache

wallet_packages=bdb

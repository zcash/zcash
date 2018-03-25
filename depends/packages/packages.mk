rust_packages := rust librustzcash
<<<<<<< HEAD
zcash_packages := libsnark libgmp libsodium

ifeq ($(host_os),linux)
	packages := boost openssl libevent zeromq $(zcash_packages) googletest googlemock
else
	packages := boost openssl libevent zeromq $(zcash_packages) googletest googlemock libcurl
endif


=======
proton_packages := proton
zcash_packages := libgmp libsodium
packages := boost openssl libevent zeromq $(zcash_packages) googletest
>>>>>>> zcash/master
native_packages := native_ccache

wallet_packages=bdb

#!/bin/sh
./autogen.sh
#CPPFLAGS='-I../../libzerocash -I../../libzerocash/depinst/include -I../../libzerocash/depinst/include/libsnark -DCURVE_BN128 -Wno-literal-suffix -std=c++11' ./configure --enable-hardening --with-incompatible-bdb
BOOST_LDFLAGS='../../libzerocash/libzerocash.a' CPPFLAGS='-I../../libzerocash -I../../boost/include -I../../libzerocash/depinst/include -I../../libzerocash/depinst/include/libsnark -DCURVE_BN128 -Wno-literal-suffix -std=c++11' ./configure --enable-hardening --with-incompatible-bdb
make AM_DEFAULT_VERBOSITY=1 "$@"

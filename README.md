Zcash
=====

https://z.cash/

Where do I begin?
-----------------

We have a guide for joining the public testnet: https://github.com/zcash/zcash/wiki/Public-Alpha-Guide

What is Zcash?
--------------

Zcash is an implementation of the "Zerocash" protocol. Based on Bitcoin's code, it intends to
offer a far higher standard of privacy and anonymity through a sophisticiated zero-knowledge
proving scheme which preserves confidentiality of transaction metadata.

**Zcash is unfinished and highly experimental.** Use at your own risk.

Participation in the Zcash project is subject to a [Code of Conduct](code_of_conduct.md).

Security Warnings
-----------------

See important security warnings in
[doc/security-warnings.md](doc/security-warnings.md).

License
-------

Zcash is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see http://opensource.org/licenses/MIT.


>>>>>>>>>>>>>>>>>>>> Komodo specific notes:

```
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python zlib1g-dev wget bsdmainutils automake

git clone https://github.com/jl777/komodo

cd komodo

./autogen.sh

./configure --with-incompatible-bdb --with-gui

./zcutil/fetch-params.sh

cp ~/.zcash-params/testnet3/z9* ~/.zcash-params

./zcutil/build.sh -j8  # -j8 uses 8 threads


Create ~/.komodo/komodo.conf:

rpcuser=bitcoinrpc

rpcpassword=password

addnode="5.9.102.210"

addnode="78.47.196.146"


Start mining:

komodo/src/komodod -gen=1 -genproclimit=1

komodo/src/komodo-cli getinfo
```


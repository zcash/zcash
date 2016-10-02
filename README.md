
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
 
 
Komodo Specific Notes
=====================
 
Dependencies
------------
 
```
#The following packages are needed:
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python zlib1g-dev wget bsdmainutils automake libboost-all-dev libssl-dev libprotobuf-dev protobuf-compiler libqt4-dev libqrencode-dev libdb++-dev
```
 
Komodo
------
 
```
git clone https://github.com/jl777/komodo
cd komodo
./autogen.sh
./configure --with-incompatible-bdb --with-gui
# This command might finish with: configure: error: libgmp headers missing. This can be ignored.
./zcutil/fetch-params.sh
cp ~/.zcash-params/testnet3/z9* ~/.zcash-params
./zcutil/build.sh -j8  # -j8 uses 8 threads - replace 8 with number of threads you want to use
#This can take some time.
```
 
Create komodo.conf
------------------
 
```
cd ~
mkdir .komodo
cd .komodo
pico komodo.conf
#Add the following lines to the komodo.conf file:

rpcuser=bitcoinrpc
rpcpassword=password
addnode=5.9.102.210
addnode=78.47.196.146
addnode=178.63.69.164
addnode=88.198.65.74
addnode=5.9.122.241
addnode=144.76.94.38
```
 
Start mining
------------
 
```
cd ~
cd komodo
#This starts komodo as a process - replace genproclimit with number of threads you want to use:
./src/komodod -gen=1 -genproclimit=1 &

#This will get the stats:
./src/komodo-cli getinfo

#To view the process:
ps -ef | grep komodod
 
#To view komodod output:
tail -f ~/.komodo/debug.log
```


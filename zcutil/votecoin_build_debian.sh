#!/usr/bin/env bash

cd "$(dirname "$(readlink -f "$0")")"    #'"%#@!

sudo apt-get install \
      build-essential pkg-config libc6-dev m4 g++-multilib \
      autoconf libtool ncurses-dev unzip git python python-zmq \
      zlib1g-dev wget bsdmainutils automake

./fetch-params.sh || exit 1

./build.sh --disable-tests --disable-rust -j$(nproc) || exit 1

if [ ! -r ~/.votecoin/votecoin.conf ]; then
   mkdir -p ~/.votecoin
   echo "addnode=mainnet.votecoin.site" >~/.votecoin/votecoin.conf
   echo "rpcuser=username" >>~/.votecoin/votecoin.conf
   echo "rpcpassword=`head -c 32 /dev/urandom | base64`" >>~/.votecoin/votecoin.conf
fi

cd ../src/
cp -f zcashd votecoind
cp -f zcash-cli votecoin-cli
cp -f zcash-tx votecoin-tx

echo ""
echo "--------------------------------------------------------------------------"
echo "Compilation complete. Now you can run ./src/votecoind to start the daemon."
echo "It will use configuration file from ~/.votecoin/votecoin.conf"
echo ""

#!/usr/bin/env bash

apt-get install \
      build-essential pkg-config libc6-dev m4 g++-multilib \
      autoconf libtool ncurses-dev unzip git python python-zmq \
      zlib1g-dev wget bsdmainutils automake

./fetch-params.sh || exit 1

./build.sh --disable-rust -j$(nproc) || exit 1

if [ ! -r ~/.votecoin/votecoin.conf ]; then
   mkdir -p ~/.votecoin
   echo "addnode=mainnet.votecoin.site" >~/.votecoin/votecoin.conf
   echo "rpcuser=username" >>~/.votecoin/votecoin.conf
   echo "rpcpassword=`head -c 32 /dev/urandom | base64`" >>~/.votecoin/votecoin.conf
fi

cd ../src/
mv zcashd votecoind
mv zcash-cli votecoin-cli
mv zcash-tx votecoin-tx

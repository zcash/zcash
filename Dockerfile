FROM debian:jessie

RUN apt-get update && apt-get install -y \
      build-essential pkg-config libc6-dev m4 g++-multilib \
      autoconf libtool ncurses-dev unzip git python \
      zlib1g-dev wget bsdmainutils automake

RUN git clone https://github.com/zcash/zcash.git /usr/local/src/zcash
WORKDIR /usr/local/src/zcash
RUN git checkout v1.0.0
RUN zcutil/fetch-params.sh

RUN zcutil/build.sh -j$(nproc)

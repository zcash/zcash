FROM ubuntu:latest
MAINTAINER alexellis2@gmail.com

RUN apt-get update -q && apt-get -qy install \
      build-essential pkg-config libc6-dev m4 g++-multilib \
            autoconf libtool ncurses-dev unzip git python \
                  zlib1g-dev wget bsdmainutils automake

WORKDIR /root/
RUN git clone https://github.com/zcash/zcash.git
WORKDIR /root/zcash/
RUN git checkout v1.0.0-rc4
RUN ./zcutil/fetch-params.sh

RUN ./zcutil/build.sh -j$(nproc)

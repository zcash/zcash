FROM debian:jessie AS builder

ENV ZCASH_URL=https://github.com/zcash/zcash.git \
    ZCASH_CONF=/home/zcash/.zcash/zcash.conf

ARG VERSION=master

RUN apt-get update

RUN apt-get -qqy install --no-install-recommends build-essential \
    automake ncurses-dev libcurl4-openssl-dev libssl-dev libgtest-dev \
    make autoconf automake libtool git apt-utils pkg-config libc6-dev \
    libcurl3-dev libudev-dev m4 g++-multilib unzip git python zlib1g-dev \
    wget ca-certificates pwgen bsdmainutils curl

WORKDIR /src
RUN git clone ${ZCASH_URL}

WORKDIR /src/zcash
RUN git checkout $VERSION
RUN ./zcutil/build.sh -j$(nproc)

WORKDIR /src/zcash/src
RUN /usr/bin/install -c zcash-tx zcashd zcash-cli zcash-gtest ../zcutil/fetch-params.sh -t /usr/bin/

FROM debian:jessie

ENV ZCASH_CONF=/home/zcash/.zcash/zcash.conf

RUN apt-get update

RUN apt-get install -y libgomp1 wget curl

RUN apt-get clean all -y

COPY --from=builder /usr/bin/zcash-cli /usr/bin/zcashd /usr/bin/fetch-params.sh /usr/bin/

RUN adduser --uid 1000 --system zcash && \
    mkdir -p /home/zcash/.zcash/ && \
    mkdir -p /home/zcash/.zcash-params/ && \
    chown -R zcash /home/zcash && \
    echo "Success"

USER zcash
RUN echo "rpcuser=zcash" > ${ZCASH_CONF} && \
        echo "rpcpassword=`head /dev/urandom | tr -dc A-Za-z0-9 | head -c 13 ; echo ''`" >> ${ZCASH_CONF} && \
        echo "addnode=mainnet.z.cash" >> ${ZCASH_CONF} && \
        echo "Success"

VOLUME ["/home/zcash/.zcash"]

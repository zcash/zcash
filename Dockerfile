FROM phusion/baseimage:0.9.19

# create user and set homedir
RUN useradd -s /bin/bash -m -d /var/lib/zcash zcash

ENV HOME /var/lib/zcash

# homedir is a data volume
VOLUME ["/var/lib/zcash"]

# install build deps
RUN \
    apt-get update && \
    apt-get install -y \
        autoconf \
        automake \
        bsdmainutils \
        build-essential \
        g++-multilib \
        git \
        libc6-dev \
        libtool \
        m4 \
        ncurses-dev \
        pkg-config \
        python \
        time \
        unzip \
        wget \
        zlib1g-dev \
    && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# add source
ADD . /usr/local/src/zcash

# download dependencies
RUN cd /usr/local/src/zcash/depends && \
    make download-linux

# build and install to /usr/local/bin
# FIXME build and run tests here too
RUN \
    mkdir -p /etc/zcash && \
    ln -s /etc/zcash $HOME/.zcash-params && \
    cd /usr/local/src/zcash && \
    bash ./zcutil/fetch-params.sh && \
    chmod a+rx /etc/zcash && \
    chmod a+r /etc/zcash/* && \
    time ./zcutil/build.sh -j$(nproc) && \
    make install && \
    cp -av ./depends/x86_64-unknown-linux-gnu/bin/* /usr/local/bin && \
    rm /var/lib/zcash/.zcash-params && \
    cd / && \
    rm -rf /usr/local/src/zcash && \
    rm -rf /var/lib/zcash/.ccache

# remove build deps
RUN \
    apt-get remove -y \
        autoconf \
        automake \
        bsdmainutils \
        build-essential \
        g++-multilib \
        git \
        libc6-dev \
        libtool \
        m4 \
        ncurses-dev \
        pkg-config \
        python \
        unzip \
        wget \
        zlib1g-dev \
    && \
    apt-get autoremove -y && \
    rm -rf \
        /var/lib/apt/lists/* \
        /tmp/* \
        /var/tmp/* \
        /var/cache/* \
        /usr/include \
        /usr/local/include

# create cache dir and set ownership
RUN mkdir /var/cache/zcash && \
    chown zcash:zcash -R /var/cache/zcash

# set ownership on keys
RUN chmod a+rX -R /etc/zcash

# set ownership on homedir contents
RUN chown zcash:zcash -R /var/lib/zcash

# rpc port
EXPOSE 8232

# p2p port
EXPOSE 8233

# set up service to run on container start
RUN mkdir -p /etc/service/zcash
ADD contrib/init/zcashd.run /etc/service/zcash/run
RUN chmod +x /etc/service/zcash/run

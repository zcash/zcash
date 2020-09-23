# Reference: https://zcash.readthedocs.io/en/latest/rtd_pages/Debian-Ubuntu-build.html "building from source"
FROM debian:stretch

RUN DEBIAN_FRONTEND=noninteractive \
        apt-get update && \
    DEBIAN_FRONTEND=noninteractive \
        apt-get install -qq \
            autoconf \
            automake \
            bsdmainutils \
            build-essential \
            curl \
            g++-multilib \
            git \
            libc6-dev \
            libtool \
            m4 \
            ncurses-dev \
            pkg-config \
            python3 \
            python3-zmq \
            unzip \
            zlib1g-dev

ARG BUILDER_UID
RUN adduser \
        --uid "$BUILDER_UID" \
        --no-create-home \
        --disabled-password \
        --gecos '' \
        builder

USER builder

ENTRYPOINT ["/workspace/zcutil/build.sh"]

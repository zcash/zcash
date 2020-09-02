ARG BASE_IMAGE
FROM $BASE_IMAGE

ARG DISTRO=debian
RUN apt-get update && apt-get --no-install-recommends -y install \
    build-essential \
    autoconf \
    cmake \
    pkg-config \
    curl \
    git \
    wget \
    libtool \
    lcov \
    valgrind \
    bsdmainutils

RUN useradd -ms /bin/bash -U $DISTRO
USER $DISTRO:$DISTRO
WORKDIR /home/$DISTRO

CMD ["sleep", "infinity"]

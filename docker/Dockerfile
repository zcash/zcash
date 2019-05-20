FROM ubuntu:16.04
LABEL Description="Ycash Build Env"

# Dependencies of the Qt offline installer
RUN apt-get -y update && apt-get install -y \
    g++ build-essential cmake wget git \
    autoconf automake autopoint gperf libtool libtool-bin intltool unzip pkg-config bsdmainutils curl


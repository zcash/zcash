FROM debian:10

RUN apt-get update \
  && apt-get install -y gnupg2 apt-transport-https curl

ARG ZCASH_SIGNING_KEY_ID=6DEF3BAF272766C0

ARG ZCASH_VERSION=
# The empty string for ZCASH_VERSION will install the apt default version,
# which should be the latest stable release. To install a specific
# version, pass `--build-arg 'ZCASH_VERSION=<version>'` to `docker build`.

ARG ZCASHD_USER=zcashd
ARG ZCASHD_UID=2001

RUN apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys $ZCASH_SIGNING_KEY_ID \
  && echo "deb [arch=amd64] https://apt.z.cash/ stretch main" > /etc/apt/sources.list.d/zcash.list \
  && apt-get update

RUN if [ -z "$ZCASH_VERSION" ]; \
    then apt-get install -y zcash; \
    else apt-get install -y zcash=$ZCASH_VERSION; \
    fi; \
    zcashd --version

RUN useradd --home-dir /srv/$ZCASHD_USER \
            --shell /bin/bash \
            --create-home \
            --uid $ZCASHD_UID\
            $ZCASHD_USER
USER $ZCASHD_USER
WORKDIR /srv/$ZCASHD_USER
RUN mkdir -p /srv/$ZCASHD_USER/.zcash/ \
    && touch /srv/$ZCASHD_USER/.zcash/zcash.conf

ADD entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

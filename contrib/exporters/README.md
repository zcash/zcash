# Quick Setup
This setup assumes Docker is installed on your system. 
Please see https://docs.docker.com/engine/install/ to install.

Also recommended: Install ctop - https://github.com/bcicen/ctop

### Creating zcashd directories?
Decide where you want to store (or refence existing ) zcashd data.

For example, create 2 new directories:
```
sudo mkdir -p /srv/zcashd/.zcash
sudo mkdir -p /srv/zcashd/.zcash-params
```

Make uid 2001 owner of the directories. This is the uid of user zcashd runs as.
```
sudo chown -R 2001 /srv/zcashd/.zcash
sudo chown -R 2001 /srv/zcashd/.zcash-params
```

### Installing Loki Docker log driver

```
docker plugin install  grafana/loki-docker-driver:latest --alias loki --grant-all-permissions
```

### Update HTTP Timeout for graceful stops
```
export COMPOSE_HTTP_TIMEOUT=500
```

### Start exporters, monitoring, and zcash
```
docker-compose up -d
```
### Useful commands
```
docker-compose up -d

docker-compose stop

docker-compose ps

docker logs -f <image-name>

docker rm <image-name>

#On a zcash testnet pod
zcash-cli -rpcuser=${ZCASHD_RPCUSER} -rpcpassword=${ZCASHD_RPCPASSWORD} -rpcport=${ZCASHD_RPCPORT} -testnet getinfo

#On a zcash mainnet pod
zcash-cli -rpcuser=${ZCASHD_RPCUSER} -rpcpassword=${ZCASHD_RPCPASSWORD} -rpcport=${ZCASHD_RPCPORT} -mainnet getinfo
```

#### ecc_zcashd_exporter

Install docker and docker-compose

`pip3 install -r requirements.txt`

Docker

time docker build -t ecc_z_exporter .

real    0m16.119s
user    0m2.458s
sys     0m0.300s

docker image ls ecc_z_exporter:latest

REPOSITORY       TAG       IMAGE ID       CREATED              SIZE
ecc_z_exporter   latest    3271c08cb24c   About a minute ago   51.5MB

docker run -it --rm ecc_z_exporter:latest

To rebuild this image you must use `docker-compose build` or `docker-compose up --build`.

## Local Docker Registry Setup
See https://docs.docker.com/registry/deploying/

1. Run local registry
```
docker run -d -p 5000:5000 --restart=always --name registry registry:2
```

2. Use modified Dockerfile to shiim in resources from host machine
```
FROM ubuntu:21.10

RUN apt-get update \
  && apt-get install -y gnupg2 apt-transport-https curl

ARG ZCASHD_USER=zcashd
ARG ZCASHD_UID=2001

########
# APT
########
#ARG ZCASH_SIGNING_KEY_ID=6DEF3BAF272766C0
#ARG ZCASH_VERSION=
# The empty string for ZCASH_VERSION will install the apt default version,
# which should be the latest stable release. To install a specific
# version, pass `--build-arg 'ZCASH_VERSION=<version>'` to `docker build`.

########
# APT
########
# RUN apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys $ZCASH_SIGNING_KEY_ID \
#   && echo "deb [arch=amd64] https://apt.z.cash/ stretch main" > /etc/apt/sources.list.d/zcash.list \
#   && apt-get update

# RUN if [ -z "$ZCASH_VERSION" ]; \
#     then apt-get install -y zcash; \
#     else apt-get install -y zcash=$ZCASH_VERSION; \
#     fi; \
#     zcashd --version

RUN useradd --home-dir /srv/$ZCASHD_USER \
            --shell /bin/bash \
            --create-home \
            --uid $ZCASHD_UID\
            $ZCASHD_USER

#######
# ADD from local resources(e.g. binaries, src, params, etc.)
#
# Note: if you use binaries ensure they are compatible with your image
# FROM ubuntu:21.10
#######
ADD zcashd /usr/local/bin
ADD zcash-cli /usr/local/bin
ADD zcash-fetch-params /usr/local/bin

USER $ZCASHD_USER
WORKDIR /srv/$ZCASHD_USER
RUN mkdir -p /srv/$ZCASHD_USER/.zcash/ \
    && touch /srv/$ZCASHD_USER/.zcash/zcash.conf

ADD entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
```
IMPORTANT NOTE: Ensure whatever you ADD into /usr/local/bin above, is in the same
path as this Dockerfile:

```
zcash/contrib/docker/Dockerfile
zcash/contrib/docker/zcashd
zcash/contrib/docker/zcash-cli
zcash/contrib/docker/zcash-fetch-params
```

docker build . -t localhost:5000/electriccoinco/zcashd:v4.7.0-rc1.1 --no-cache

docker tag electriccoinco/zcashd:v4.7.0-rc1.1 localhost:5000/electriccoinco/zcashd:v4.7.0-rc1.1

docker push localhost:5000/electriccoinco/zcashd:v4.7.0-rc1.1

If you get an error on the push command, remove the image hash for the registry and restart it:
```
mdr0id@pop-os:~/Code/nu5_zcash/zcash/contrib/docker$ docker run -d -p 5000:5000 --name registry registry:2
docker: Error response from daemon: Conflict. The container name "/registry" is already in use by container "76ea0528921aabe83534c0502bb943c75f99ba57aacb58120aba90c74675cf05". You have to remove (or rename) that container to be able to reuse that name.
See 'docker run --help'.

mdr0id@pop-os:~/Code/nu5_zcash/zcash/contrib/docker$ docker rm 76ea0528921aabe83534c0502bb943c75f99ba57aacb58120aba90c74675cf05

mdr0id@pop-os:~/Code/nu5_zcash/zcash/contrib/docker$ docker run -d -p 5000:5000 --name registry registry:2

```

4. Update your docker-compose.yml to contain your local image name you just pushed:

```
---
version: '2'

services:
  zcashd:
    image: localhost:5000/electriccoinco/zcashd:v4.7.0-rc1.1
    #OR use a standard image from our dockerhub
    #image: electriccoinco/zcashd:latest
    volumes:
      - $ZCASHD_DATADIR:/srv/zcashd/.zcash
      - $ZCASHD_PARMDIR:/srv/zcashd/.zcash-params
    env_file:
      - .env
    ports:
      - "$ZCASHD_RPCPORT:$ZCASHD_RPCPORT"
      - "$ZCASHD_PROMETHEUSPORT:$ZCASHD_PROMETHEUSPORT"
```

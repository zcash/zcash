#! /usr/bin/env sh

docker build --disable-content-trust=false -t registry.gitlab.com/zingo-labs/zcash/packer:1.0 - < Dockerfile.packerrunner &&
docker push registry.gitlab.com/zingo-labs/zcash/packer:1.0

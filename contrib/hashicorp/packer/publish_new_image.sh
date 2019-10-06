#! /usr/bin/env sh

docker build --disable-content-trust=false -t $1 - < Dockerfile.packerrunner &&
docker push $1

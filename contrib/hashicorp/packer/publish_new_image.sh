#! /usr/bin/env sh
IMAGE_NAME=$1
docker build --disable-content-trust=false -t $IMAGE_NAME - < Dockerfile.packerrunner &&
docker push $IMAGE_NAME

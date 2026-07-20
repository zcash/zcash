#docker build .  --build-arg BASE_IMAGE=debian:8 -t base-jessie-amd64:latest
#docker build .  --build-arg BASE_IMAGE=debian:9 -t base-stretch-amd64:latest
#docker build .  --build-arg BASE_IMAGE=debian:10 -t base-buster-amd64:latest
docker build .  --build-arg BASE_IMAGE=ubuntu:xenial \
  --build-arg DISTRO=ubuntu -t base-xenial-amd64:latest

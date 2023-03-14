#!/usr/bin/env bash
# /************************************************************************
#  File: docker-build.sh
#  Author: mdr0id
#  Date: 8/23/22
#  Description:  
#  Used to build production CI images with Docker infrastructure.
#
#  zcashd-build-platform = image that only builds zcashd
#
#  zcashd-worker-platform = image that can build and run all targets in 
#  full_test_suite.py
#  
#  zcashd-bench-platform = image that can build zcashd, run all targets in
#  full_test_suite.py, and all targets in performance-measurements.sh
#
#  To build a zcash worker, please move ~/.zcash-params to this directory  
#
#  To build a zcash bench, please move benchmark-200k-UTXOs.tar.xz and 
#  block-107134.tar.xz to this directory
#
#  Important note: using buildx mechanism of docker allows given images to
#  build for other/multiple architectures:
#  docker buildx build --platform=linux/amd64,linux/arm64 . 
#  
# ************************************************************************/

export LC_ALL=C
set -exo pipefail

# Debian 9
docker build . -f Dockerfile-build-python.apt --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=9 -t electriccoinco/zcashd-build-debian9
docker push electriccoinco/zcashd-build-debian9

docker build . -f Dockerfile-tekton-worker --build-arg BASEOS=debian --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=9 -t electriccoinco/zcashd-worker-debian9
docker push electriccoinco/zcashd-worker-debian9

docker build . -f Dockerfile-tekton-bench --build-arg BASEOS=debian --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=9 -t electriccoinco/zcashd-bench-debian9
docker push electriccoinco/zcashd-bench-debian9

# Debian 10
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=10 -t electriccoinco/zcashd-build-debian10
docker push electriccoinco/zcashd-build-debian10

docker build . -f Dockerfile-tekton-worker --build-arg BASEOS=debian --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=10 -t electriccoinco/zcashd-worker-debian10
docker push electriccoinco/zcashd-worker-debian10

docker build . -f Dockerfile-tekton-bench --build-arg BASEOS=debian --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=10 -t electriccoinco/zcashd-bench-debian10
docker push electriccoinco/zcashd-bench-debian10

# Debian 11
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=bullseye -t electriccoinco/zcashd-build-debian11
docker push electriccoinco/zcashd-build-debian11

docker build . -f Dockerfile-tekton-worker --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=11 -t electriccoinco/zcashd-worker-debian11
docker push electriccoinco/zcashd-worker-debian11

docker build . -f Dockerfile-tekton-bench --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=11 -t electriccoinco/zcashd-bench-debian11
docker push electriccoinco/zcashd-bench-debian11

# Ubuntu 16.04
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=16.04 -t electriccoinco/zcashd-build-ubuntu1604
docker push electriccoinco/zcashd-build-ubuntu1604

docker build . -f Dockerfile-tekton-worker --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=1604 -t electriccoinco/zcashd-worker-ubuntu1604
docker push electriccoinco/zcashd-worker-ubuntu1604

docker build . -f Dockerfile-tekton-bench --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=1604 -t electriccoinco/zcashd-bench-ubuntu1604
docker push electriccoinco/zcashd-bench-ubuntu1604

# Ubuntu 18.04
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=18.04 -t electriccoinco/zcashd-build-ubuntu1804
docker push electriccoinco/zcashd-build-ubuntu1804

docker build . -f Dockerfile-tekton-worker --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=1804 -t electriccoinco/zcashd-worker-ubuntu1804
docker push electriccoinco/zcashd-worker-ubuntu1804

docker build . -f Dockerfile-tekton-bench --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=1804 -t electriccoinco/zcashd-bench-ubuntu1804
docker push electriccoinco/zcashd-bench-ubuntu1804

# Ubuntu 20.04
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=20.04 -t electriccoinco/zcashd-build-ubuntu2004
docker push electriccoinco/zcashd-build-ubuntu2004

docker build . -f Dockerfile-tekton-worker --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=2004 -t electriccoinco/zcashd-worker-ubuntu2004
docker push electriccoinco/zcashd-worker-ubuntu2004

docker build . -f Dockerfile-tekton-bench --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=2004 -t electriccoinco/zcashd-bench-ubuntu2004
docker push electriccoinco/zcashd-bench-ubuntu2004

# Ubuntu 22.04
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=22.04 -t electriccoinco/zcashd-build-ubuntu2204
docker push electriccoinco/zcashd-build-ubuntu2204

docker build . -f Dockerfile-tekton-worker --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=2204 -t electriccoinco/zcashd-worker-ubuntu2004
docker push electriccoinco/zcashd-worker-ubuntu2204

docker build . -f Dockerfile-tekton-bench --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=2204 -t electriccoinco/zcashd-bench-ubuntu2204
docker push electriccoinco/zcashd-bench-ubuntu2204

# Centos8
docker build . -f Dockerfile-build.centos8 -t electriccoinco/zcashd-build-centos8
docker push electriccoinco/zcashd-build-centos8

docker build . -f Dockerfile-tekton-worker --build-arg FROMBASEOS=centos --build-arg FROMBASEOS_BUILD_TAG=8  -t electriccoinco/zcashd-worker-centos8
docker push electriccoinco/zcashd-worker-centos8

docker build . -f Dockerfile-tekton-bench --build-arg FROMBASEOS=centos --build-arg FROMBASEOS_BUILD_TAG=8  -t electriccoinco/zcashd-bench-centos8
docker push electriccoinco/zcashd-bench-centos8

# Arch 20210418.0.20194
docker build . -f Dockerfile-build.arch --build-arg FROMBASEOS=archlinux --build-arg FROMBASEOS_BUILD_TAG=base-20210418.0.20194 -t electriccoinco/zcashd-build-archlinux
docker push electriccoinco/zcashd-build-archlinux

docker build . -f Dockerfile-tekton-worker --build-arg FROMBASEOS=archlinux -t electriccoinco/zcashd-worker-archlinux
docker push electriccoinco/zcashd-worker-archlinux

docker build . -f Dockerfile-tekton-bench --build-arg FROMBASEOS=archlinux -t electriccoinco/zcashd-bench-archlinux
docker push electriccoinco/zcashd-bench-archlinux
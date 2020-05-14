# Debian
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=9 -t electriccoinco/zcashd-build:debian9
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=10 -t electriccoinco/zcashd-build:debian10

docker push electriccoinco/zcashd-build:debian9
docker push electriccoinco/zcashd-build:debian10

docker build . -f Dockerfile-gitian.apt --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=9 -t electriccoinco/zcashd-gitian:debian9
docker build . -f Dockerfile-gitian.apt --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=10 -t electriccoinco/zcashd-gitian:debian10

docker push electriccoinco/zcashd-gitian:debian9
docker push electriccoinco/zcashd-gitian:debian10

docker build . -f Dockerfile-bbworker.apt --build-arg BASEOS=debian --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=9 -t electriccoinco/zcashd-bbworker:debian9
docker build . -f Dockerfile-bbworker.apt --build-arg BASEOS=debian --build-arg FROMBASEOS=debian --build-arg FROMBASEOS_BUILD_TAG=10 -t electriccoinco/zcashd-bbworker:debian10

docker push electriccoinco/zcashd-bbworker:debian9
docker push electriccoinco/zcashd-bbworker:debian10

# Ubuntu
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=18.04 -t electriccoinco/zcashd-build:ubuntu18.04
docker build . -f Dockerfile-build.apt --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=20.04 -t electriccoinco/zcashd-build:ubuntu20.04

docker push electriccoinco/zcashd-build:ubuntu18.04
docker push electriccoinco/zcashd-build:ubuntu20.04

docker build . -f Dockerfile-gitian.apt --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=18.04 -t electriccoinco/zcashd-gitian:ubuntu18.04
docker build . -f Dockerfile-gitian.apt --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=20.04 -t electriccoinco/zcashd-gitian:ubuntu20.04

docker push electriccoinco/zcashd-gitian:ubuntu18.04
docker push electriccoinco/zcashd-gitian:ubuntu20.04

docker build . -f Dockerfile-bbworker.apt --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=18.04 -t electriccoinco/zcashd-bbworker:ubuntu18.04
docker build . -f Dockerfile-bbworker.apt --build-arg BASEOS=ubuntu --build-arg FROMBASEOS=ubuntu --build-arg FROMBASEOS_BUILD_TAG=20.04 -t electriccoinco/zcashd-bbworker:ubuntu20.04

docker push electriccoinco/zcashd-bbworker:ubuntu18.04
docker push electriccoinco/zcashd-bbworker:ubuntu20.04
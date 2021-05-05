# zcashd ci Docker images

These Dockerfiles can be used to build zcashd.

The current objective is to build a base image for each distribution that includes the system packages to build zcashd. From `build` images, more targeted images are created.

The process is meant to be automated, but an example `docker-build.sh` script is included.


## build images
`apt-package-tekton-list.txt` contains the required packages for debian based systems.

`Dockerfile-build.apt` uses that file, and some build time arguments, to build apt based build images.

Currently available images are hosted at
https://hub.docker.com/r/electriccoinco/zcashd-build/tags



## Tekton worker images

`Dockerfile-tekton-worker` uses the above build images as a base and layers on toolchains needed for testing

- requirements.txt is the python package requirements for the tekton worker

Currently available images are hosted at https://hub.docker.com/r/electriccoinco


### Stand alone, best effort images

Additional Tekton base builders for Centos8 and Arch. Can be used with `Dockerfile-tekton-worker` to create Tekton workers.

- Dockerfile-build.arch
- Dockerfile-build.centos8


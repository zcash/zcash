# What

This code provides developers with the ability to produce Virtual Machine Images with minimal effort.

It's possible to simply install `packer` using your package manager.  The approach outlined below and committed here,
provides a shareable snapshot of the tool, along with code-as-documentation detailing its build.
This code is included in two ways, first the tooling and commands producing the build are committed-and-signed
here, second the source code that the binary is produced from is stored in a public-signed
docker registry image.   TODO:  confirm docker signature, and check whether additional push is required for a
multistage build.

Included features:

* minimal prerequisites:
     - a development environment that includes Docker (see below about low-privilege "dockerapp" user)
     - sufficient authority for resource allocation (e.g. AWS key and key ID), within service provider
* maximal auditability; where possible content-based hashes are included in the code
     - TODO: Find and verify the signature, of the official golang image
          The base image is the "Official" golang image as described here:
https://hub.docker.com/layers/golang/library/golang/latest/images/sha256-a50a9364e9170ab5f5b03389ed33b9271b4a7b6bbb0ab41c4035adb3078927bc
     - The "source_ami" is from this list: https://wiki.debian.org/Cloud/AmazonEC2Image/Buster
* portability:
     - Produced Docker images are small enough to transfer conveniently (TODO ~ XXX MB)
* optionality:
     - Build it yourself, or use the standard shared-signed public image
* composeability:
    - The tool is intended to be a component of a more complete CI solution
    - The included gitlabrunner config, is only one example of use

# How

## Running (Run this only if you trust me!  You can check my GPG signature on the commits.)

### Setup a development environment with docker.

  This implies a certain posture towards the UID owning the "docker" process. It is risky to run
  containers as the `root` user on a system.
  
  The approach this tool takes is to always apply the `--user=$UID` flag to `docker run` commands.
  
  There are at least 2 ways one can use this pattern to reduce the privileges of the container process.
  One option is to pass one's own UID to the run command, e.g. `docker run --user=$(echo $UID)`.

  I opted instead to create a User dedicated to running docker, "dockerapp", after which I created a local `tmp`
  directory (as required by `packer build`), and set its ownership to `dockerapp`:

  1. `useradd -u $AUIDNUMBER dockerapp`
  2. `mkdir ./tmp && chown dockerapp:dockerapp ./tmp`

This directory will be written to by `packer build` commands.  It is mounted into the container
  when it is run (with `--user=$AUIDNUMBER`).

### Build the AMI.

```BASH
./packer_build.sh build aws_access_key_id=$KEYID aws_secret_key=$SECRETKEY ssh_pubkey=$SSHPUBKEY_INAWSDIRECTORY aws/gitlabrunner.json
```

Building
--------
1. Build the runner (NOTE: this step works for my gitlab container registry, you'll need your own
or we'll need to change permissions):

```BASH
cd ./contrib/devtools/packer && \
./publish_new_image.sh registry.gitlab.com/zingo-labs/zcash/packer:1.0
```

# Context

  Some container info, I found useful:
  
  #. https://docs.docker.com/develop/develop-images/dockerfile_best-practices/
  #. https://www.youtube.com/watch?reload=9&v=sJx_emIiABk
  #. https://medium.com/@mccode/understanding-how-uid-and-gid-work-in-docker-containers-c37a01d01cf
  #. https://medium.com/@chemidy/create-the-smallest-and-secured-golang-docker-image-based-on-scratch-4752223b7324

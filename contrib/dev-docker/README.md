# zcash development Dockerfile

This Dockerfile provides the build toolchain for building zcashd via `/zcutil/build.sh'. The entrypoint is that build script, so that anyone with docker can do development builds without installing the dev toolchain directly.

## Wrapper script

A wrapper script `./run.sh` will build and run the image with the appropriate bind mount and other settings so that the resulting change to the repo directory is the same as if a user had run `./zcutil/build.sh` with the dev toolchain installed locally.

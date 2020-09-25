# Using Docker

## Build host requirements

### Linux
- git
- docker
- ruby
- lintian
- ??

## Build target docker images

gitian-builder expects a local docker image for each suite with names in the format: `base-<suite>-<architecture>`

Build these required images on the host performing the gitian build with `./docker/make_base_vm.sh`, construct and tag the images in some other way.

This is taking the place up the gitian-build function https://github.com/devrandom/gitian-builder/blob/master/bin/make-base-vm.

**WITH THIS DIRECTORY AS CURRENT WORKING DIRECOTRY**
```
cd docker/ && ./make_base_vm.sh
```

## Perform a gitian-build

The only required env var is `USE_DOCKER`, but for scripting, something like this should be used:

**WITH THIS DIRECTORY AS CURRENT WORKING DIRECOTRY**
```
export USE_DOCKER=1 
export JOBS=12 
export ZCASH_COMMIT=docker-gitian-builder
export ZCASH_REPO=https://github.com/benzcash/zcash.git
```

Then run it like
```
git clone https://github.com/devrandom/gitian-builder
cd gitian-builder
mkdir -p {cache/common,var,build}
bin/gbuild -j4 \
  --commit zcash=${ZCASH_COMMIT} \
  --url zcash=${ZCASH_REPO} \
  ../gitian-linux.yml
```

### Check result

```
head result/zcash-3.1.0-rc2-res.yml
--- !!omap
- out_manifest: |
    cfeb7dfb7369b9d596c3d14547556cd2ae1e6e1ffc0ed24c5e37e4574c39281f  src/zcash-3.1.0.tar.gz
    2563c654390cbabb9fc641137b93da5e784e949dc0f2ed0e008c9a0583b54b9c  zcash-3.1.0-linux64-debug.tar.gz
    c3a7c844b7d71be716e641d7ca4c458e8b510febe1ced4364e6a3c39344cbbb5  zcash-3.1.0-linux64.tar.gz
- in_manifest: |-
    3f70d727a13984b1f3d63ee2b133548ba3708223108eb2db01e5a596db6f6f92  zcash-3.1.0-rc2-desc.yml
    git:9395a270d3a511f4b166407f1d341c5a53f86820 zcash
- base_manifests: !!omap
  - xenial-amd64: |
```


### Look good? Sign it!

Set an env var of the gpg key you want to use to sign.

```
export SIGNER='ben@electriccoin.co'
```

Still in `zcash/contrib/gitian-descriptors/gitian-builder`, run
```
git clone git@github.com:zcash/gitian.sigs.git
./bin/gsign -p "gpg --detach-sign" \
  --signer "$SIGNER" \
  --release ${ZCASH_COMMIT}_xenial-ubuntu \
  --destination gitian.sigs \
  ../gitian-linux.yml
```

### Create debian package

Extract built binaries from output to the git repo that
gitian-builder created in `inputs`.

```
tar -zvxf build/out/zcash-3.1.0-linux64.tar.gz \
  -C inputs/zcash/src/ \
  --strip-components=2 \
  zcash-3.1.0/bin/zcash-cli \
  zcash-3.1.0/bin/zcash-tx \
  zcash-3.1.0/bin/zcashd \
  zcash-3.1.0/bin/zcash-fetch-params
```

Change to that git repo and create a debian package with the repo script.

```
cd inputs/zcash
./zcutil/build-debian-package.sh
```

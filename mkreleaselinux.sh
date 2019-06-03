#!/bin/bash

echo "Building Linux"
make clean >/dev/null
./zcutil/build.sh -j$(nproc) >/dev/null
strip src/ycashd
strip src/ycash-cli
cp src/ycashd ../ycash/artifacts/
cp src/ycash-cli ../ycash/artifacts/


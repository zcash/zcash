#!/bin/bash

echo "Building Windows"
make clean > /dev/null
HOST=x86_64-w64-mingw32 ./zcutil/build.sh -j8 >/dev/null
strip src/ycashd.exe
strip src/ycash-cli.exe
cp src/ycashd.exe ../ycash/artifacts/
cp src/ycash-cli.exe ../ycash/artifacts/


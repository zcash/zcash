#!/bin/bash
## Usage:
##  ./build_DEBIAN_package.sh

set -e
set -x

BUILD_PATH="/tmp/zcbuild"
PACKAGE_NAME="zcash"
SRC_PATH=`pwd`

umask 022

if [ ! -d $BUILD_PATH ]; then
    mkdir $BUILD_PATH
fi

PACKAGE_VERSION=$(grep Version $SRC_PATH/contrib/DEBIAN/control | cut -d: -f2 | tr -d ' ')
BUILD_DIR="$BUILD_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64"

if [ -d $BUILD_DIR ]; then
    rm -R $BUILD_DIR
fi
mkdir -p $BUILD_DIR/DEBIAN $BUILD_DIR/usr/bin

cp -r $SRC_PATH/contrib/DEBIAN/* $BUILD_DIR/DEBIAN/
cp $SRC_PATH/src/zcashd $BUILD_DIR/usr/bin/
cp $SRC_PATH/src/zcash-cli $BUILD_DIR/usr/bin/
cp $SRC_PATH/zcutil/fetch-params.sh $BUILD_DIR/usr/bin/zcash-fetch-params

# Create the deb package
dpkg-deb --build $BUILD_DIR
cp $BUILD_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64.deb $SRC_PATH

exit 0

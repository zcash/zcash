#!/bin/bash
## Usage:
##  ./zcutil/build-debian-package.sh

set -e
set -x

BUILD_PATH="/tmp/verus-cli"
PACKAGE_NAME="verus-cli"
SRC_PATH=`pwd`
SRC_DEB=$SRC_PATH/contrib/debian
SRC_DOC=$SRC_PATH/doc

umask 022

if [ ! -d $BUILD_PATH ]; then
    mkdir $BUILD_PATH
fi

## PACKAGE_VERSION=$($SRC_PATH/src/zcashd --version | grep version | cut -d' ' -f4 | tr -d v)
## Need version setting from environment

PACKAGE_VERSION=0.3.10-beta

##
## Also, what does the sed end up doing?
DEBVERSION=$(echo $PACKAGE_VERSION | sed 's/-beta/~beta/' | sed 's/-rc/~rc/' | sed 's/-/+/')
BUILD_DIR="$BUILD_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64"

if [ -d $BUILD_DIR ]; then
    rm -R $BUILD_DIR
fi

DEB_BIN=$BUILD_DIR/usr/bin
DEB_CMP=$BUILD_DIR/usr/share/bash-completion/completions
DEB_DOC=$BUILD_DIR/usr/share/doc/$PACKAGE_NAME
DEB_MAN=$BUILD_DIR/usr/share/man/man1
mkdir -p $BUILD_DIR/DEBIAN $DEB_CMP $DEB_BIN $DEB_DOC $DEB_MAN
chmod 0755 -R $BUILD_DIR/*

# Package maintainer scripts (currently empty)
#cp $SRC_DEB/postinst $BUILD_DIR/DEBIAN
#cp $SRC_DEB/postrm $BUILD_DIR/DEBIAN
#cp $SRC_DEB/preinst $BUILD_DIR/DEBIAN
#cp $SRC_DEB/prerm $BUILD_DIR/DEBIAN
# Copy binaries
cp $SRC_PATH/src/komodod $DEB_BIN
strip $DEB_BIN/komodod
cp $SRC_PATH/src/verusd $DEB_BIN
cp $SRC_PATH/src/komodo-cli $DEB_BIN
strip $DEB_BIN/komodo-cli
cp $SRC_PATH/src/verus $DEB_BIN
cp $SRC_PATH/zcutil/fetch-params.sh $DEB_BIN/zcash-fetch-params
# Copy docs
cp $SRC_PATH/doc/release-notes/release-notes-1.0.0.md $DEB_DOC/changelog
cp $SRC_DEB/changelog $DEB_DOC/changelog.Debian
cp $SRC_DEB/copyright $DEB_DOC
cp -r $SRC_DEB/examples $DEB_DOC
# Copy manpages
cp $SRC_DOC/man/komodod.1 $DEB_MAN
cp $SRC_DOC/man/komodo-cli.1 $DEB_MAN
cp $SRC_DOC/man/zcash-fetch-params.1 $DEB_MAN
# Copy bash completion files
cp $SRC_PATH/contrib/zcashd.bash-completion $DEB_CMP/zcashd
cp $SRC_PATH/contrib/zcash-cli.bash-completion $DEB_CMP/zcash-cli
# Gzip files
gzip --best -n $DEB_DOC/changelog
gzip --best -n $DEB_DOC/changelog.Debian
gzip --best -n $DEB_MAN/komodod.1
gzip --best -n $DEB_MAN/komodo-cli.1
gzip --best -n $DEB_MAN/zcash-fetch-params.1

cd $SRC_PATH/contrib

# Create the control file
dpkg-shlibdeps $DEB_BIN/komodod $DEB_BIN/komodo-cli
dpkg-gencontrol -P$BUILD_DIR -v$DEBVERSION

# Create the Debian package
fakeroot dpkg-deb --build $BUILD_DIR
cp $BUILD_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64.deb $SRC_PATH
# Analyze with Lintian, reporting bugs and policy violations
lintian -i $SRC_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64.deb
exit 0

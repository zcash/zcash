#!/bin/sh
# find the dylibs to copy for komodo-cli
DYLIBS=`otool -L src/komodo-cli | grep "/usr/local" | awk -F' ' '{ print $1 }'`
echo "copying $DYLIBS to $src"
# copy the dylibs to the srcdir
for dylib in $DYLIBS; do cp -rf $dylib src/; done

# modify komodo-cli to point to dylibs
echo "modifying $binary to use local libraries"
for dylib in $DYLIBS; do install_name_tool -change $dylib @executable_path/`basename $dylib` src/komodo-cli; done;
chmod +x src/komodo-cli
#
# find the dylibs to copy for komodod
DYLIBS=`otool -L src/komodod | grep "/usr/local" | awk -F' ' '{ print $1 }'`
echo "copying $DYLIBS to $src"
# copy the dylibs to the srcdir
for dylib in $DYLIBS; do cp -rf $dylib src/; done

# modify komodod to point to dylibs
echo "modifying $binary to use local libraries"
for dylib in $DYLIBS; do install_name_tool -change $dylib @executable_path/`basename $dylib` src/komodod; done;
chmod +x src/komodod


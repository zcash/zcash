#!/bin/sh

KMD_DIR=kmd/mac/verus-cli
binaries=("komodo-cli" "komodod")

for binary in "${binaries[@]}";
do
  # find the dylibs to copy for komodod
    DYLIBS=`otool -L src/$binary | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying $DYLIBS to $KMD_DIR"
    # copy the dylibs to the srcdir
    for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; done

    # modify komodod to point to dylibs
    echo "modifying $binary to use local libraries"
    for dylib in $DYLIBS; do install_name_tool -change $dylib @executable_path/`basename $dylib` $KMD_DIR/$binary; done;
    chmod +x $KMD_DIR/$binary
done

libraries=("libgcc_s.1.dylib" "libgomp.1.dylib" "libidn2.0.dylib" "libstdc++.6.dylib")

for binary in "${libraries[@]}";
do
    # Need to undo this for the dylibs when we are done
    chmod 755 $KMD_DIR/$binary
    # find the dylibs to copy for komodod
    DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying indirect $DYLIBS to $KMD_DIR"
    # copy the dylibs to the srcdir
    for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; done
    echo "modifying komodod to use local libraries"
    for dylib in $DYLIBS; do install_name_tool -change $dylib @executable_path/`basename $dylib` $KMD_DIR/komodod; done;
    chmod +x $KMD_DIR/komodod
    for dylib in $DYLIBS; do install_name_tool -change $dylib @executable_path/`basename $dylib` $KMD_DIR/komodo-cli; done;
    chmod +x $KMD_DIR/komodo-cli
done

indirectlibraries=("libintl.8.dylib" "libunistring.2.dylib")

for binary in "${indirectlibraries[@]}";
do
    # Need to undo this for the dylibs when we are done
    chmod 755 src/$binary
    # find the dylibs to copy for komodod
    DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying $DYLIBS to $KMD_DIR"
    # copy the dylibs to the srcdir
    for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; done
    # fix binary references to it
    echo "modifying komodod to use local libraries"
    for dylib in $DYLIBS; do install_name_tool -change $dylib @executable_path/`basename $dylib` $KMD_DIR/komodod; done;
    chmod +x $KMD_DIR/komodod
    for dylib in $DYLIBS; do install_name_tool -change $dylib @executable_path/`basename $dylib` $KMD_DIR/komodo-cli; done;
    chmod +x $KMD_DIR/komodo-cli
done


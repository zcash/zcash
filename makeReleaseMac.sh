#!/bin/sh

KMD_DIR=kmd/mac/verus-cli

binaries=("komodo-cli" "komodod")
alllibs=()
for binary in "${binaries[@]}";
do
    # do the work in the destination directory
    cp src/$binary $KMD_DIR
    # find the dylibs to copy for komodod
    DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying $DYLIBS to $KMD_DIR"
    # copy the dylibs to the srcdir
    for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; done
    #DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/lib" | awk -F' ' '{ print $1 }'`
    # copy the other dylibs to the srcdir
    #for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; alllibs+=($dylib); done
done

libraries=("libgcc_s.1.dylib" "libgomp.1.dylib" "libidn2.0.dylib" "libstdc++.6.dylib")

for binary in "${libraries[@]}";
do
    # Need to undo this for the dylibs when we are done
    chmod 755 $KMD_DIR/$binary
    # find the dylibs to copy for komodod
    DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying $DYLIBS to $KMD_DIR"
    # copy the dylibs to the srcdir
    for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; alllibs+=($dylib); done
    #DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/lib" | awk -F' ' '{ print $1 }'`
    # copy the other dylibs to the srcdir
    #for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; alllibs+=($dylib); newlibs+=(`basename $dylib`); done
done

indirectlibraries=("libintl.8.dylib" "libunistring.2.dylib")

newlibs=()
for binary in "${indirectlibraries[@]}"
do
    # Need to undo this for the dylibs when we are done
    chmod 755 src/$binary
    # find the dylibs to copy for komodod
    DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying indirect $DYLIBS to $KMD_DIR"
    # copy the dylibs to the dest dir
    for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; alllibs+=($dylib); done
    #DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/lib" | awk -F' ' '{ print $1 }'`
    # copy the other dylibs to the srcdir
    #for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; alllibs+=($dylib); newlibs+=(`basename $dylib`); done
done

# now process all the new libs we found indirectly
for binary in "${newlibs[@]}";
do
    # Need to undo this for the dylibs when we are done
    chmod 755 src/$binary
    # find the dylibs to copy for komodod
    DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying indirect $DYLIBS to $KMD_DIR"
    # copy the dylibs to the dest dir
    for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; alllibs+=($dylib); done
    #DYLIBS=`otool -L $KMD_DIR/$binary | grep "/usr/lib" | awk -F' ' '{ print $1 }'`
    # copy the other dylibs to the srcdir
    #for dylib in $DYLIBS; do cp -rf $dylib $KMD_DIR; alllibs+=($dylib); done
done

for binary in "${binaries[@]}";
do
    # modify komododi or komodo-cli to point to dylibs
    echo "modifying $binary to use local libraries"
    for dylib in "${alllibs[@]}"
    do
        echo "Next lib is $dylib "
        install_name_tool -change $dylib @executable_path/`basename $dylib` $KMD_DIR/$binary
    done
    chmod +x $KMD_DIR/$binary
done

libs=("${libraries[@]}" "${indirectlibraries[@]}" "${newlibs[@]}")
for binary in "${libs[@]}";
do
    # modify dylibs to point to dylibs
    echo "modifying $binary to use local libraries"
    for dylib in "${alllibs[@]}"
    do
        echo "Next lib is $dylib "
        chmod 755 $KMD_DIR/$binary
        install_name_tool -change $dylib @executable_path/`basename $dylib` $KMD_DIR/$binary
    done
    chmod +x $KMD_DIR/$binary
done


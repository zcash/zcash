#!/bin/sh

PACKAGE_DIR="$@"
mkdir ${PACKAGE_DIR}

binaries=("komodo-cli" "komodod")
alllibs=()
for binary in "${binaries[@]}";
do
    # do the work in the destination directory
    cp src/${binary} ${PACKAGE_DIR}
    # find the dylibs to copy for komodod
    DYLIBS=`otool -L ${PACKAGE_DIR}/${binary} | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying ${DYLIBS} to ${PACKAGE_DIR}"
    # copy the dylibs to the srcdir
    for dylib in ${DYLIBS}; do cp -rf ${dylib} ${PACKAGE_DIR}; done
done

libraries=("libgcc_s.1.dylib" "libgomp.1.dylib" "libidn2.0.dylib" "libstdc++.6.dylib")

for binary in "${libraries[@]}";
do
    # find the dylibs to copy for komodod
    DYLIBS=`otool -L ${PACKAGE_DIR}/${binary} | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying ${DYLIBS} to ${PACKAGE_DIR}"
    # copy the dylibs to the srcdir
    for dylib in ${DYLIBS}; do cp -rf ${dylib} ${PACKAGE_DIR}; alllibs+=(${dylib}); done
done

indirectlibraries=("libintl.8.dylib" "libunistring.2.dylib")

for binary in "${indirectlibraries[@]}";
do
    # Need to undo this for the dylibs when we are done
    chmod 755 src/${binary}
    # find the dylibs to copy for komodod
    DYLIBS=`otool -L ${PACKAGE_DIR}/${binary} | grep "/usr/local" | awk -F' ' '{ print $1 }'`
    echo "copying indirect ${DYLIBS} to ${PACKAGE_DIR}"
    # copy the dylibs to the dest dir
    for dylib in ${DYLIBS}; do cp -rf ${dylib} ${PACKAGE_DIR}; alllibs+=(${dylib}); done
done

for binary in "${binaries[@]}";
do
    # modify komodod to point to dylibs
    echo "modifying ${binary} to use local libraries"
    for dylib in "${alllibs[@]}"
    do
        echo "Next lib is ${dylib} "
        install_name_tool -change ${dylib} @executable_path/`basename ${dylib}` ${PACKAGE_DIR}/${binary}
    done
    chmod +x ${PACKAGE_DIR}/${binary}
done

for binary in "${libraries[@]}";
do
    # modify libraries to point to dylibs
    echo "modifying ${binary} to use local libraries"
    for dylib in "${alllibs[@]}"
    do
        echo "Next lib is ${dylib} "
        install_name_tool -change ${dylib} @executable_path/`basename ${dylib}` ${PACKAGE_DIR}/${binary}
    done
done

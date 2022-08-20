#!/usr/bin/env bash

export LC_ALL=C
set -o pipefail

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
CLANG_PACKAGE="$SCRIPT_DIR/../../depends/packages/native_clang.mk"
LIBCXX_PACKAGE="$SCRIPT_DIR/../../depends/packages/libcxx.mk"

CLANG_VERSION=$( grep -v _major_version $CLANG_PACKAGE | grep -oP "_version=\K.*" )
LIBCXX_MSYS2_VERSION=$( grep -oP "_msys2_version=\K.*" $LIBCXX_PACKAGE )

update_clang_hash() {
    url="https://github.com/llvm/llvm-project/releases/download/llvmorg-$CLANG_VERSION/clang+llvm-$CLANG_VERSION-$1.tar.xz"
    echo "Fetching $url"
    hash=$( curl -fL $url | sha256sum | awk '{print $1}' )
    retVal=$?
    if [ $retVal -ne 0 ]; then
        if [ $retVal -eq 22 ]; then
            echo
            echo "The LLVM project has not published a $CLANG_VERSION build for $1."
            echo "You will need to manually fix the Makefile to use a different version."
            echo
        fi
    else
        sed -i "/\$(package)_sha256_hash_$2=/c\\\$(package)_sha256_hash_$2=$hash" $CLANG_PACKAGE
        sed -i "/\$(package)_sha256_hash_$2=/c\\\$(package)_sha256_hash_$2=$hash" $LIBCXX_PACKAGE
    fi
}

update_libcxx_msys2_hash() {
    url="https://repo.msys2.org/mingw/x86_64/mingw-w64-x86_64-$1-$LIBCXX_MSYS2_VERSION-any.pkg.tar.zst"
    echo "Fetching $url"
    hash=$( curl -fL $url | sha256sum | awk '{print $1}' )
    sed -i "/\$(package)_$2=/c\\\$(package)_$2=$hash" $LIBCXX_PACKAGE
}

# For native targets
# update_clang_hash CLANG_COMPILED_TARGET MAKEFILE_PACKAGE_IDENTIFIER
update_clang_hash aarch64-linux-gnu aarch64_linux
update_clang_hash x86_64-apple-darwin darwin
update_clang_hash x86_64-linux-gnu-ubuntu-18.04 linux
update_clang_hash amd64-unknown-freebsd12 freebsd

# For Windows cross-compilation
# update_libcxx_msys2_hash LIBCXX_LIBRARY MAKEFILE_HASH_SUFFIX
update_libcxx_msys2_hash libc++ sha256_hash
update_libcxx_msys2_hash libc++abi libcxxabi_sha256_hash

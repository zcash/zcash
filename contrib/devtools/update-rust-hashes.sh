#!/usr/bin/env bash

export LC_ALL=C

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
RUST_PACKAGE="$SCRIPT_DIR/../../depends/packages/native_rust.mk"

RUST_VERSION=$( cat $RUST_PACKAGE | grep -oP "_version=\K.*" )

update_hash() {
    url="https://static.rust-lang.org/dist/$1-$RUST_VERSION-$2.tar.gz"
    echo "Fetching $url"
    hash=$( curl $url | sha256sum | awk '{print $1}' )
    sed -i "/\$(package)_$3_$4=/c\\\$(package)_$3_$4=$hash" $RUST_PACKAGE
}

update_rust_hash() {
    update_hash rust $1 sha256_hash $2
}

update_stdlib_hash() {
    update_hash rust-std $1 rust_std_sha256_hash $1
}

# For native targets
# update_rust_hash RUST_TARGET MAKEFILE_PACKAGE_IDENTIFIER
update_rust_hash aarch64-unknown-linux-gnu aarch64_linux
update_rust_hash x86_64-apple-darwin darwin
update_rust_hash x86_64-unknown-linux-gnu linux
update_rust_hash x86_64-unknown-freebsd freebsd

# For cross-compilation targets
# update_stdlib_hash RUST_TARGET
update_stdlib_hash aarch64-unknown-linux-gnu
update_stdlib_hash x86_64-apple-darwin
update_stdlib_hash x86_64-pc-windows-gnu
update_stdlib_hash x86_64-unknown-freebsd

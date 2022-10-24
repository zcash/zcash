{ autoreconfHook
, boost
, ctaes
, cxx-rs
, db62
, gtest
, hexdump
, leveldb
, lib
, libevent
, librustzcash
, libsodium
, llvmPackages
, makeWrapper
, openssl
, pkg-config
, python
, secp256k1
, src
, tinyformat
, univalue
, utf8cpp
, zeromq
, zk-parameters
}:

llvmPackages.stdenv.mkDerivation {
  inherit src;

  pname = "zcash";
  version = "5.3.0";

  buildInputs = [
    boost
    ctaes
    db62
    gtest
    leveldb
    libevent
    librustzcash
    libsodium
    openssl
    secp256k1
    tinyformat
    univalue
    utf8cpp
    zeromq
  ];

  nativeBuildInputs = [
    autoreconfHook
    cxx-rs
    hexdump
    makeWrapper
    pkg-config
    python
    zk-parameters
  ];

  # I think this is needed because the “utf8cpp” dir component is non-standard,
  # but I don’t know why the package doesn’t set that up correctly.
  CXXFLAGS = "-I${ctaes} -I${tinyformat} -I${utf8cpp}/include/utf8cpp";

  # Because of fetch-params, everything expects the parameters to be in `HOME`.
  HOME = "${zk-parameters}/var/cache";

  # So we can override the paths to libraries provided by the “depends/” build
  # in the Makefile.
  NIX_LIBLEVELDB = leveldb;
  NIX_LIBRUSTZCASH = librustzcash;
  NIX_LIBSECP256K1 = secp256k1;
  NIX_LIBUNIVALUE = univalue;

  patches = [
    ./patches/autoreconf/make-nix-friendly.patch
    ./patches/zcash/ctaes.patch
    ./patches/zcash/tinyformat.patch
  ];

  postPatch = ''
    substituteInPlace ./src/Makefile.am \
      --subst-var NIX_LIBLEVELDB \
      --subst-var NIX_LIBRUSTZCASH \
      --subst-var NIX_LIBSECP256K1 \
      --subst-var NIX_LIBUNIVALUE
    patchShebangs ./src/test/bitcoin-util-test.py
  '';

  configureFlags = [ "--with-boost-libdir=${boost}/lib" ];

  doCheck = true;
}

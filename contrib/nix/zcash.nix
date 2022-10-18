{ autoreconfHook
, boost
, cxx-rs
, db62
, gtest
, hexdump
, libevent
, librustzcash
, libsodium
, llvmPackages_14
, makeWrapper
, openssl
, pkg-config
, python3
, utf8cpp
, zeromq
, zk-parameters
}:

llvmPackages_14.stdenv.mkDerivation {
  pname = "zcash";
  version = "5.3.0";

  src = ../..;

  buildInputs = [
    boost
    db62
    gtest
    libevent
    librustzcash
    (libsodium.overrideAttrs (old: {
      patches = old.patches ++ [
        ../../depends/patches/libsodium/1.0.15-pubkey-validation.diff
        ../../depends/patches/libsodium/1.0.15-signature-validation.diff
      ];
    }))
    openssl
    utf8cpp
    (zeromq.overrideAttrs (old: {
      patches = [
        ../../depends/patches/zeromq/windows-unused-variables.diff
      ];
    }))
  ];

  nativeBuildInputs = [
    autoreconfHook
    cxx-rs
    hexdump
    makeWrapper
    pkg-config
    python3
    zk-parameters
  ];

  # I think this is needed because the “utf8cpp” dir component is non-standard,
  # but I don’t know why the package doesn’t set that up correctly.
  CXXFLAGS = "-I${utf8cpp}/include/utf8cpp";

  # Because of fetch-params, everything expects the parameters to be in `HOME`.
  HOME = "${zk-parameters}/var/cache";

  postPatch = ''
    patchShebangs ./src/test/bitcoin-util-test.py
  '';

  configureFlags = [
    "--with-boost-libdir=${boost}/lib"
    "--with-rustzcash-dir=${librustzcash}"
  ];

  doCheck = true;
}

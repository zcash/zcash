{
  autoreconfHook,
  boost,
  ctaes,
  cxx-rs,
  db,
  gtest,
  hexdump,
  leveldb,
  lib,
  libevent,
  librustzcash,
  libsodium,
  llvmPackages,
  makeWrapper,
  openssl,
  pkg-config,
  python,
  secp256k1,
  src,
  tinyformat,
  univalue,
  utf8cpp,
  zeromq,
  zk-parameters,
}:
llvmPackages.stdenv.mkDerivation {
  inherit src;

  pname = "zcash";
  version = "5.3.0";

  buildInputs = [
    boost
    ctaes
    db
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

  # Because of fetch-params, everything expects the parameters to be in `HOME`.
  HOME = "${zk-parameters}/share/zcash";

  patches = [
    ./patches/autoreconf/make-nix-friendly.patch
    ./patches/zcash/ctaes.patch
    ./patches/zcash/tinyformat.patch
    ./patches/zcash/utf8cpp.patch
  ];

  postPatch = ''
    # Overrides the paths to libraries provided by the “depends/” build in the
    # Makefile.
    substituteInPlace ./src/Makefile.am \
      --subst-var-by NIX_LIBLEVELDB "${leveldb}" \
      --subst-var-by NIX_LIBRUSTZCASH "${librustzcash}" \
      --subst-var-by NIX_LIBSECP256K1 "${secp256k1}" \
      --subst-var-by NIX_LIBUNIVALUE "${univalue}"
    patchShebangs ./contrib/devtools/security-check.py
  '';

  configureFlags = ["--with-boost=${boost}"];

  doCheck = true;

  postCheck = ''
    make -C ./src check-security
  '';

  postInstall = ''
    COMPLETIONS_DIR=$out/share/bash-completion/completions
    mkdir -p "$COMPLETIONS_DIR"
    cp ./contrib/zcashd.bash-completion $COMPLETIONS_DIR/zcashd
    cp ./contrib/zcash-cli.bash-completion $COMPLETIONS_DIR/zcash-cli
    cp ./contrib/zcash-tx.bash-completion $COMPLETIONS_DIR/zcash-tx
  '';

  doInstallCheck = true;
}

{
  checksec,
  mkShell,
  openssl,
  python,
  src,
  zcash,
}:
mkShell {
  inherit src;

  patches = [
    # • disables rust-test, as that’s done in librustzcash;
    # • disables util-test, as that’s done in zcash;
    # • disables no-dot-so, secp256k1, and univalue because they’re
    #   not involved in the build;
    # • skips `make check-security` and performs that in the `zcash`
    #   package instead, since it’s integrated with `make`; and
    # • skips `test_rpath_runpath` because we do expect to have a
    #   runpath now, since everything is pinned via nixpkgs.
    ./patches/full_test_suite/make-nix-friendly.patch
  ];

  nativeBuildInputs = [
    checksec
    openssl
    (python.withPackages (pypkgs: [
      pypkgs.pyzmq
      pypkgs.simplejson
    ]))
    zcash
  ];

  # Needed by p2p-fullblocktest. Python is bad at finding shared
  # libs.
  LD_LIBRARY_PATH = "${openssl.out}/lib";
}

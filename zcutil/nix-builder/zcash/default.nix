with import ./../pkgs-pinned.nix;
let
  zcboost = import ./deps/boost;
in
  stdenv.mkDerivation {
    pname = "zcash";
    version = "FIXME";
    src = ./../../..;
    nativeBuildInputs = [
      autoreconfHook
      file
      hexdump
      pkg-config

      # Zcash-custom dependencies:
      zcboost
      (import ./deps/bdb)
      (import ./deps/libevent)
      (import ./deps/libsodium)
      (import ./deps/openssl)
    ];

    configureFlags = [
      "--with-boost=${zcboost}"
    ];

    # Patch absolute paths from libtool to use nix file:
    # See https://github.com/NixOS/nixpkgs/issues/98440
    preConfigure = ''sed -i 's,/usr/bin/file,${file}/bin/file,g' ./configure'';
  }

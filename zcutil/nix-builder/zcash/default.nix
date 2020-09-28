let
  pkgs = import ./../pkgs-pinned.nix;

  # Build dependencies from nixpkgs:
  nixdeps = with pkgs; [
      autoreconfHook
      file
      git
      hexdump
      pkg-config
  ];

  # Zcash-custom dependencies:
  zcboost = import ./deps/boost;
  zcdeps = [
      zcboost
      (import ./deps/bdb)
      (import ./deps/libevent)
      (import ./deps/libsodium)
      (import ./deps/openssl)
      (import ./deps/utfcpp)
    ];
in
  pkgs.stdenv.mkDerivation {
    pname = "zcash";
    version = "FIXME";
    src = ./../../..;
    nativeBuildInputs = nixdeps ++ zcdeps;

    configureFlags = [
      "--with-boost=${zcboost}"
    ];

    # Patch absolute paths from libtool to use nix file:
    # See https://github.com/NixOS/nixpkgs/issues/98440
    preConfigure = ''sed -i 's,/usr/bin/file,${pkgs.file}/bin/file,g' ./configure'';
  }

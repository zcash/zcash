let
  pkgs = import ./../pkgs-pinned.nix;
in
  with pkgs;
  stdenv.mkDerivation {
    pname = "zcash";
    version = "FIXME";
    src = ./../../..;
    nativeBuildInputs = [
      autoreconfHook
      pkg-config
      hexdump

      # Zcash-pinned dependencies:
      (import ./deps/bdb)
      (import ./deps/openssl)
    ];
  }

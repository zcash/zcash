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
    ];
  }

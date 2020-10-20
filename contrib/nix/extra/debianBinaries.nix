let
  inherit (import ../util) nixpkgs mkInternalDerivation;
  inherit (nixpkgs) fetchurl;
in
  mkInternalDerivation {
    subname = "fetched-debian-binaries";
    src = fetchurl {
      url = "https://z.cash/downloads/zcash-4.0.0-linux64-debian-stretch.tar.gz";
      sha256 = "a0daf673d45e92fe97f2dd43bbaf6d6653940643aff62915f46df89af4d8c8b5";
    };
    builder = ''
      source "$stdenv/setup"
      set -x
      tar -xf "$src"
      mv ./zcash-4.0.0 "$out"
      set +x
    '';
  }

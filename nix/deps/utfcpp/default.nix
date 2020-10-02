let
  inherit (import ./../../util) nixpkgs;
  inherit (nixpkgs) stdenv;
  sources = import ./../../sources;
in
  stdenv.mkDerivation rec {
    pname = "utfcpp";
    version = "3.1";
    src = "${sources}/${pname}-${version}.tar.gz";
    builder = ./builder.sh;
  }

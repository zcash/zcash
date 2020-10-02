let
  inherit (import ./../../util) nixpkgs requirePlatform;
  inherit (nixpkgs) stdenv;
  sources = import ./../../sources;
in
  stdenv.mkDerivation rec {
    pname = "googletest";
    version = "1.8.0";
    src = "${sources}/${pname}-${version}.tar.gz";

    cxxflags = [
      "-std=c++11"
      (requirePlatform "[^-]*-[^-]*-linux-gnu" "-fPIC")
    ];

    builder = ./builder.sh;
  }



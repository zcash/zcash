let
  inherit (builtins) getAttr;
  inherit (import ./../../util) nixpkgs requirePlatform;
  inherit (nixpkgs) stdenv buildPlatform;
  sources = import ./../../sources;

  pname = "rust";
  version = "1.44.1";
  platform = (
    requirePlatform "x86_64-unknown-linux-gnu" buildPlatform.config
  );
in
  stdenv.mkDerivation rec {
    name = "${pname}-${version}-${platform}";
    src = "${sources}/${name}.tar.gz" ;
    nativeBuildInputs = [ nixpkgs.file ];
    builder = ./builder.sh;
  }

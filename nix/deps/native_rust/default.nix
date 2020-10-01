let
  inherit (builtins) getAttr;
  inherit (import ./../../util) nixpkgs requirePlatform patchDir;
  inherit (nixpkgs) stdenv fetchurl buildPlatform;

  pname = "rust";
  version = "1.44.1";
  platform = buildPlatform.config;
in
  stdenv.mkDerivation rec {
    name = "${pname}-${version}-${platform}";
    src = fetchurl {
      url = "https://static.rust-lang.org/dist/${name}.tar.gz";

      # BUG: I've only tested on "x86_64-unknown-linux-gnu"
      sha256 = getAttr platform {
        "x86_64-unknown-linux-gnu" = "a41df89a461a580536aeb42755e43037556fba2e527dd13a1e1bb0749de28202";
        "x86_64-apple-darwin" = "a5464e7bcbce9647607904a4afa8362382f1fc55d39e7bbaf4483ac00eb5d56a";
        "x86_64-unknown-freebsd" = "36a14498f9d1d7fb50d6fc01740960a99aff3d4c4c3d2e4fff2795ac8042c957";
      };
    };

    nativeBuildInputs = [ nixpkgs.file ];
    builder = ./builder.sh;
  }

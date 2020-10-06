let
  inherit (import ../util) nixpkgs srcDir config importTOML;
  inherit (nixpkgs) stdenv lib;
  inherit (config) zcash;
  inherit (zcash) pname;

  version = import ../version.nix;
  fetchCrate = import ./fetchCrate.nix;
  rustpkgname = (importTOML (srcDir + "/Cargo.toml")).package.name;
  rustpkgs = (importTOML (srcDir + "/Cargo.lock")).package;
  externalpkgs =
    lib.lists.filter ({name, ...}: name != rustpkgname) rustpkgs;
in
  stdenv.mkDerivation {
    name = "${pname}-${version}-vendored-crates";
    crates = map fetchCrate externalpkgs;
    builder = ./builder.sh;
  }

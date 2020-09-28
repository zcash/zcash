# FIXME: This version does not support cross-builds.
with {
  inherit (import ./../../zcstd.nix)
    attrNames
    fetchurl
    importTOML
    lib
    mkDerivation
    zcname
    zcsrc
    zcversion
    zcbuildutil
  ; 
};
let
  fetchCrate = import ./fetch-crate.nix;
  rustpkgname = (importTOML "${zcsrc}/Cargo.toml").package.name;
  rustpkgs = (importTOML "${zcsrc}/Cargo.lock").package;
  externalpkgs =
    lib.lists.filter ({name, ...}: name != rustpkgname) rustpkgs;
in
  mkDerivation {
    name = "${zcname}-${zcversion}-vendored-crates";
    crates = map fetchCrate externalpkgs;
    builder = ./builder.sh;
    zcbuildutil = zcbuildutil;
  }

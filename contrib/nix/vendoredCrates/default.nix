let
  inherit (import ../util) mkInternalDerivation nixpkgs srcDir importTOML;
  inherit (nixpkgs) lib;

  fetchCrate = import ./fetchCrate.nix;
  rustpkgname = (importTOML (srcDir + "/Cargo.toml")).package.name;
  rustpkgs = (importTOML (srcDir + "/Cargo.lock")).package;
  externalpkgs =
    lib.lists.filter ({name, ...}: name != rustpkgname) rustpkgs;
in
  mkInternalDerivation {
    subname = "vendored-crates";
    crates = map fetchCrate externalpkgs;
    builder = ./builder.sh;
  }

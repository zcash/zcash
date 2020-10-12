let
  inherit (import ../util) mkInternalDerivation nixpkgs selectSource importTOML;
  inherit (nixpkgs) lib;

  fetchCrate = import ./fetchCrate.nix;
  metadataSource = selectSource "cargo-metadata" [
    "Cargo.toml"
    "Cargo.lock"
  ];
  rustpkgname = (importTOML (metadataSource + "/Cargo.toml")).package.name;
  rustpkgs = (importTOML (metadataSource + "/Cargo.lock")).package;
  externalpkgs =
    lib.lists.filter ({name, ...}: name != rustpkgname) rustpkgs;
in
  mkInternalDerivation {
    subname = "vendored-crates";
    crates = map fetchCrate externalpkgs;
    builder = ./builder.sh;
  }

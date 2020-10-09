let
  inherit (import ../util) nixpkgs srcDir idevName importTOML zcstdenv;
  inherit (nixpkgs) lib;

  fetchCrate = import ./fetchCrate.nix;
  rustpkgname = (importTOML (srcDir + "/Cargo.toml")).package.name;
  rustpkgs = (importTOML (srcDir + "/Cargo.lock")).package;
  externalpkgs =
    lib.lists.filter ({name, ...}: name != rustpkgname) rustpkgs;
in
  zcstdenv.mkDerivation {
    name = idevName "vendored-crates";
    crates = map fetchCrate externalpkgs;
    builder = ./builder.sh;
  }

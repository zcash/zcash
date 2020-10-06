let
  inherit (import ./nixpkgs.nix) lib;
  inherit (lib.attrsets) mapAttrsToList;
  importTOML = import ./importTOML.nix;
  parseSource = import ./parseSource.nix;

  # Pattern matching only works for function calls, so we define a set
  # of functions to do schema checks:
  parseTOML = { zcash, nixpkgs, dependencies }:
    {
      zcash = parseZcash zcash;
      nixpkgs = parseNixPkgs nixpkgs;
      dependencies = mapAttrsToList parseSource dependencies;
    };

  # The {} @ x: x idiom returns the set unchanged if it matches the pattern:
  parseZcash = { pname, version, fallbackUrl } @ good: good;
  parseNixPkgs = { gitrev, sha256 } @ good: good;
in
  parseTOML (importTOML ./../config.toml)

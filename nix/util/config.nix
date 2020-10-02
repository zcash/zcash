let
  inherit (import ./nixpkgs.nix) lib;
  inherit (lib.attrsets) mapAttrsToList;
  
  # Pattern matching only works for function calls, so we define a set
  # of functions to do schema checks:
  parseTOML = { zcash, nixpkgs, dependencies, sources }:
    {
      zcash = parseZcash zcash;
      nixpkgs = parseNixPkgs nixpkgs;
      dependencies = mapAttrsToList parseDependency dependencies;
      sources = map parseSource sources;
    };

  # The {} @ x: x idiom returns the set unchanged if it matches the pattern:
  parseZcash = { pname, version, fallbackUrl } @ good: good;
  parseNixPkgs = { gitrev, sha256 } @ good: good;
  parseSource = { url, sha256 } @ good: good;

  # This one changes a set of entries { name: { ...dependencyAttrs } }
  # into a list of dependencyAttrs with `pname` set to the name.
  parseDependency = name: value:
    value // { pname = name; };

  importTOML = import ./importTOML.nix;
  rawTOML = importTOML ./../config.toml;
in
  parseTOML rawTOML

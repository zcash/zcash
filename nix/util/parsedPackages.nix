let
  importTOML = import ./importTOML.nix;
  readDirBySuffix = import ./readDirBySuffix.nix;
  parseSource = import ./parseSource.nix;
  pkgsDir = ../packages;

  tomlFiles = readDirBySuffix ".toml" pkgsDir;
in
  builtins.mapAttrs (n: v: parseSource n (importTOML v)) tomlFiles

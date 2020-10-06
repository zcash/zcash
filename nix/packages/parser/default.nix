let
  inherit (import ../../util) importTOML readDirBySuffix;
  parseSource = import ./parseSource.nix;
  pkgsDir = ./..;

  tomlFiles = readDirBySuffix ".toml" pkgsDir;
in
  builtins.mapAttrs (n: v: parseSource n (importTOML v)) tomlFiles

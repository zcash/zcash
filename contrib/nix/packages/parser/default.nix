{
  allowInconsistency ? false,
}:
let
  pkgsDir = ./..;

  inherit (import ../../util) importTOML readDirBySuffix;
  parseSource = import ./parseSource.nix;
  checkConsistency = import ./checkConsistency.nix;

  tomlFiles = readDirBySuffix ".toml" pkgsDir;
  nixPackages = builtins.mapAttrs (n: v: parseSource n (importTOML v)) tomlFiles;
in
  assert checkConsistency {
    inherit nixPackages allowInconsistency;
  };
  nixPackages

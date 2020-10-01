let
  inherit (builtins) attrNames listToAttrs pathExists readDir;
  inherit (import ./nixpkgs.nix) lib;
  inherit (lib.attrsets) filterAttrs;
  inherit (lib.strings) hasSuffix removeSuffix;
in
  dirpath:
    let
      dirEntries = readDir dirpath;
      isNixFile = name: kind: hasSuffix ".nix" name && kind == "regular";
      isNixDir = name: kind: kind == "directory" && pathExists (dirpath + "/${name}/default.nix");
      isNixMod = name: kind: (isNixFile name kind) || (isNixDir name kind);
      nixFiles = attrNames (filterAttrs isNixMod dirEntries);
      importEntry = fname: {
        name = removeSuffix ".nix" fname;
        value = import (dirpath + "/${fname}");
      };
    in
      listToAttrs (map importEntry nixFiles)

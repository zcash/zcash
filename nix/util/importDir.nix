let
  inherit (builtins) attrNames listToAttrs readDir;
  inherit (import ./nixpkgs.nix) lib;
  inherit (lib.attrsets) filterAttrs;
  inherit (lib.strings) hasSuffix removeSuffix;
in
  dirpath:
    let
      dirEntries = readDir dirpath;
      isNixFile = name: kind: hasSuffix ".nix" name && kind == "regular";
      nixFiles = attrNames (filterAttrs isNixFile dirEntries);
      importEntry = fname: {
        name = removeSuffix ".nix" fname;
        value = import (dirpath + "/${fname}");
      };
    in
      listToAttrs (map importEntry nixFiles)

let
  inherit (builtins) attrNames listToAttrs readDir;
  inherit (import ./nixpkgs.nix) lib;
  inherit (lib.attrsets) filterAttrs;
  inherit (lib.strings) hasSuffix removeSuffix;
in
  suffix: dirpath: 
    let
      fileHasSuffix = name: kind: hasSuffix suffix name && kind == "regular";
      matchingFiles = attrNames (filterAttrs fileHasSuffix (readDir dirpath));
      mkEntry = fname: {
        name = removeSuffix suffix fname;
        value = dirpath + "/${fname}";
      };
    in
      listToAttrs (map mkEntry matchingFiles)


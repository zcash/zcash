let
  inherit (builtins) pathExists readDir;
  inherit (import ./nixpkgs.nix) lib;
  inherit (lib.attrsets) mapAttrs;
  filterMapAttrs = import ./filterMapAttrs.nix;  
  readDirBySuffix = import ./readDirBySuffix.nix;  
in
  dirpath:
    let
      nixFiles = readDirBySuffix ".nix" dirpath;

      nixDirs =
        let
          isNixDir = name: kind: kind == "directory" && pathExists (dirpath + "/${name}/default.nix");
          mapDir = n: k: if isNixDir n k then n else null;
        in
          filterMapAttrs mapDir (readDir dirpath);
    in
      mapAttrs (_: import) (nixDirs // nixFiles)

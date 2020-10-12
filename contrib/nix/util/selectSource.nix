# Select specific subpaths from the repo root:
let
  nixpkgs = import ./nixpkgs.nix;
  inherit (nixpkgs.lib.lists) any;
  inherit (nixpkgs.lib.strings) hasPrefix removePrefix;

  rawSrcDir = ../../..;
  localRawSrcPrefix = "${toString rawSrcDir}/";
  relPathOf = pstr: removePrefix localRawSrcPrefix pstr;
in
  name: subPaths:
    let
    in 
      builtins.path {
        name = name;
        path = rawSrcDir;
        filter = path: kind:
          let
            relPath = relPathOf path;
            matchesSubPath = subp:
              let
                subpSelectsRelPathParent = hasPrefix subp relPath;
                relPathIsSubpAncestor = hasPrefix "${relPath}/" subp;
              in
                subpSelectsRelPathParent || relPathIsSubpAncestor;
          in
            any matchesSubPath subPaths;
      }

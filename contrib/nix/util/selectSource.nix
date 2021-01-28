# Select specific subpaths from the repo root:
let
  nixpkgs = import ./nixpkgs.nix;
  inherit (nixpkgs.lib.lists) any;
  inherit (nixpkgs.lib.strings) hasPrefix removePrefix;

  rawSrcDir = ../../..;
  localRawSrcPrefix = "${toString rawSrcDir}/";
  relPathOf = pstr: removePrefix localRawSrcPrefix pstr;
in
  name: root: subPaths:
    builtins.path {
      name = name;
      path = rawSrcDir + "/${root}";
      filter =
        if subPaths == "*"
        then _: _: true
        else
          assert builtins.isList subPaths;
            path: kind:
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

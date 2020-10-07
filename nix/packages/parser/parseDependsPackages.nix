let
  dependsPackagesDir = ./../../../depends/packages;

  inherit (builtins) mapAttrs removeAttrs;
  inherit (import ../../util) readDirBySuffix;

  makeFiles = readDirBySuffix ".mk" dependsPackagesDir;

  # Remove the framework file:
  pkgFiles = removeAttrs makeFiles [ "packages" ];

  parseMakeFileHash = name: path:
    let
      inherit (builtins) head length match readFile;
      text = readFile path;
      matches = match ".*\n[^\n]*_sha256_hash=([^\n]*)\n.*" text;
    in
      assert length matches == 1;
      head matches;

in
  mapAttrs parseMakeFileHash pkgFiles

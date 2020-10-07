let
  dependsPackagesDir = ../../../../depends/packages;

  inherit (builtins) mapAttrs removeAttrs;
  inherit (import ../../util) readDirBySuffix nixpkgs;

  makeFiles = readDirBySuffix ".mk" dependsPackagesDir;

  # Remove the framework file:
  pkgFiles = removeAttrs makeFiles [ "packages" ];

  parseMakeFileHash = name: path:
    let
      inherit (builtins) head length match readFile tail;
      kernel = nixpkgs.buildPlatform.parsed.kernel.name;

      text = readFile path;
      matches = match ".*\n[^\n]*_sha256_hash(_${kernel})?=([a-f0-9]*)\n.*" text;
    in
      assert length matches == 2;
      head (tail matches);

in
  mapAttrs parseMakeFileHash pkgFiles

## This requires minimal configuration and mostly pulls the information from the
## relevant .mk file.
pkgs: let
  readSHA = infix: contents:
    builtins.elemAt
    (builtins.match ".*\\$\\(package\\)${infix}_sha256_hash=([^\n]*).*" contents)
    0;
  readVersion = infix: contents:
    builtins.elemAt
    (builtins.match ".*\\$\\(package\\)${infix}_version=([^\n]*).*" contents)
    0;
  readPatches = dependsPkgName: infix: contents: let
    patchList =
      builtins.match ".*\\$\\(package\\)${infix}_patches=([^\n]*).*" contents;
  in
    if patchList == null
    then []
    else
      map
      (patch: ../../../../depends/patches/${dependsPkgName}/${patch})
      (pkgs.lib.splitString " " (builtins.elemAt patchList 0));
  fileContents = dependsPkgName:
    builtins.readFile ../../../../depends/packages/${dependsPkgName}.mk;
in
  {
    pkg,
    dependsPkgName ? pkg.pname,
    url,
    infix ? "",
  }: let
    contents = fileContents dependsPkgName;
  in
    pkg.overrideAttrs (old: rec {
      version = readVersion infix contents;
      src = pkgs.fetchurl {
        url = url old.pname version;
        sha256 = readSHA infix contents;
      };
      patches = readPatches dependsPkgName infix contents;
    })

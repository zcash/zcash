## This requires minimal configuration and mostly pulls the information from the
## relevant .mk file.
pkgs: let
  readField = regex: infix: contents: let
    match =
      builtins.match ".*\\$\\(package\\)${infix}_${regex}=([^\n]*).*" contents;
  in
    if match == null
    then null
    else builtins.elemAt match 0;
  readDownloadFile = readField "download_file";
  readDownloadPath = readField "download_path";
  readFileName = readField "file_name";
  readSHA = readField "sha256_hash";
  readURL = infix: contents: dependsPkgName: version: let
    downloadFile = readDownloadFile infix contents;
  in
    builtins.replaceStrings
    ["$(package)" "$($(package)_version)"]
    [dependsPkgName version]
    (readDownloadPath infix contents
      + "/"
      + (
        if downloadFile == null
        then readFileName infix contents
        else downloadFile
      ));
  readVersion = readField "version";
  readPatches = dependsPkgName: infix: contents: let
    patchList = readField "patches" infix contents;
  in
    if patchList == null
    then []
    else
      map
      (patch: ../../../../depends/patches/${dependsPkgName}/${patch})
      (pkgs.lib.splitString " " patchList);
  fileContents = dependsPkgName:
    builtins.readFile ../../../../depends/packages/${dependsPkgName}.mk;
in
  {
    pkg,
    dependsPkgName ? pkg.pname,
    url ? null,
    infix ? "",
  }: let
    contents = fileContents dependsPkgName;
  in
    pkg.overrideAttrs (old: rec {
      version = readVersion infix contents;
      src = pkgs.fetchurl {
        url =
          if url == null
          then readURL infix contents dependsPkgName version
          else url dependsPkgName version;
        sha256 = readSHA infix contents;
      };
      patches = readPatches dependsPkgName infix contents;
    })

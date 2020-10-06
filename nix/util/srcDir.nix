let
  inherit (builtins) any filterSource;

  # Parent of util is nix.
  # Parent of nix is srcDir:
  rawSrcDir = ../..;

  skipPaths = [
    ".git"
    "nix"
  ];

  selectPath = path: kind:
    !(any (skipPath: path == rawSrcDir + "/${skipPath}") skipPaths);

in
  filterSource selectPath rawSrcDir

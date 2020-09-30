let
  inherit (builtins) fromTOML readFile;
in
  path: fromTOML (readFile path)

let
  inherit (import ./nixpkgs.nix) lib;
  inherit (lib.strings) concatStringsSep splitString;
in
  {
    replace = old: new: s: concatStringsSep old (splitString new s);
  }

let
  inherit (builtins) mapAttrs;
  inherit (import ./nixpkgs.nix) lib;
  inherit (lib.attrsets) filterAttrs;

  valueNotNull = _: v: v != null;
in
  f: attrs: filterAttrs valueNotNull (mapAttrs f attrs)

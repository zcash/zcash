let
  pkgname = import ./pkgname.nix;
in
  subname: "${pkgname}.${subname}"

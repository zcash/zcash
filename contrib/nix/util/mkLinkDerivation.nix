let
  inherit (builtins) attrValues;
  pkgname = import ./pkgname.nix;
  mkInternalDerivation = import ./mkInternalDerivation.nix;
in
  name: sources: mkInternalDerivation {
    inherit sources;
    subname = name;
    builder = ''
      source "$stdenv/setup"

      mkdir "$out"
      for source in $sources
      do
        linkname="$(echo "$source" | sed 's/^[^-]*-//; s/^${pkgname}\.//')"
        ln -sv "$source" "$out/$linkname"
      done
    '';
  }

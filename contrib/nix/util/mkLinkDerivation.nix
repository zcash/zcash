let
  inherit (builtins) attrValues;
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
        ln -sv "$source" "$out/$(echo "$source" | sed 's/^[^-]*-//')"
      done
    '';
  }

# Intermediate derivations are building blocks of the zcash derivation
# which aren't external dependencies. Some examples might be the vendored
# crate sources or a config.site file for autoconf.
let
  inherit (import ./nixpkgs.nix) stdenv lib;
  inherit (import ./config.nix) zcash;
in
  componentName: derivationAttrs: stdenv.mkDerivation {
    pname = "${zcash.name}-${componentName}";
    version = zcash.version;
    inherit derivationAttrs;
  }

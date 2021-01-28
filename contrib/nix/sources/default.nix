# This derivation fetchs all external sources and links them in one
# output, because nix doesn't have a --download-only option.
#
# If you want to prepare for an off-line build, do `nix build
# ./nix/sources` while online, then a subsequent `nix build ./nix` should
# work offline.
#
# Ref: https://github.com/NixOS/nix/issues/1248
let
  inherit (builtins) attrValues;
  inherit (import ../util) mkLinkDerivation nixpkgsSource;

  zcashFilteredSource = import ./zcashFilteredSource.nix;
  vendoredCrates = import ../vendoredCrates;
  packages = import ../packages { allowInconsistency = true; };
  nonCrateSources = map ({src, ...}: src) (attrValues packages);
  sources = nonCrateSources ++ [
    zcashFilteredSource
    vendoredCrates
    nixpkgsSource
  ];
in
  mkLinkDerivation "sources" sources

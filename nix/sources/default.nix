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
  inherit (import ../util) fetchurlWithFallback flip idevName nixpkgs;
  inherit (nixpkgs) stdenv;

  vendoredCrates = import ../vendoredCrates;
  packages = import ../packages;
  nonCrateSources = map ({src, ...}: src) (attrValues packages);
in
  stdenv.mkDerivation {
    name = idevName "sources";
    sources = nonCrateSources ++ [ vendoredCrates ];
    builder = ./builder.sh;
  }

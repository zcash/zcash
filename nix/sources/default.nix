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
  inherit (import ../util) config fetchurlWithFallback flip nixpkgs;
  inherit (nixpkgs) stdenv;

  vendoredCrates = import ../vendoredCrates;
  deps = import ../deps/all.nix;
  nonCrateSources = map ({src, ...}: src) (attrValues deps);
in
  stdenv.mkDerivation {
    name = "${config.zcash.pname}-${config.zcash.version}-sources";
    sources = nonCrateSources ++ [ vendoredCrates ];
    builder = ./builder.sh;
  }

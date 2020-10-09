# Define a nix "stdenv" interface supporting Zcash-specific needs,
# specifically pinning to clang.
#
# To use this, every package needs to pass the result of this import as
# `stdenv`.
let
  inherit (import ./nixpkgs.nix) llvmPackages_8;
in
  llvmPackages_8.stdenv

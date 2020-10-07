# Use this to "mark" which values are non-cross and platform specific.
#
# FIXME: Only non-cross compilation on linux x86_64 is currently supported.
let
  inherit (import ./nixpkgs.nix) buildPlatform hostPlatform;
in
  platformRegex: value:
    assert (buildPlatform == hostPlatform);
    let inherit (builtins) isList match;
    in assert (isList (match platformRegex buildPlatform.config));
    value

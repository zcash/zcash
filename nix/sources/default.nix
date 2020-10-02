# This derivation fetchs all external sources and links them in one
# output, which provides two benefits:
#
# First, it helps centralize all external source info (url & hash)
# for easy review by anyone, including those unfamiliar with nix.
#
# Second, nix doesn't yet have a "--download-only" or "--download-first"
# option. (FIXME: link to the relevant ticket). This approach allows a
# dev do have that functionality: for example, they can run `nix build
# ./nix/sources.nix` before a long no-wifi flight, then run the rest of
# the build on the flight. Also, nix builds dependencies before dependants,
# but the order isn't "leaves first", so with the more typical approach of
# `fetchurl` in each derivation, the build interleaves downloading with
# building steps. This design ensures all downloading happens early in
# the build process.
let
  inherit (import ./../util) config nixpkgs flip;
  inherit (nixpkgs) stdenv;

  fetchurlWithFallback = import ./fetchurlWithFallback.nix;
  vendoredCrates = import ./vendoredCrates;

  oldSources = map fetchurlWithFallback config.sources;
  newSources = flip map config.dependencies ({url, sha256, ...}:
    fetchurlWithFallback {
      inherit url sha256;
    }
  );
  sources = oldSources ++ newSources ++ [ vendoredCrates ];
in
  stdenv.mkDerivation {
    inherit sources;
    name = "${config.zcash.pname}-${config.zcash.version}-sources";
    builder = ./builder.sh;
  }

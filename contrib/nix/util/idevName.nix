# Create an "internal derivation" name.
# These are derivations which are not external packages yet are useful stepping stones to zcash build.
let
  inherit (import ./config.nix) zcash;
  version = import ../version.nix;
in
  idevDesc: "${zcash.pname}-${version}.${idevDesc}"

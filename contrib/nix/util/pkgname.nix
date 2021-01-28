let
  inherit (import ./config.nix) zcash;
  version = import ../version.nix;
in
  "${zcash.pname}-${version.string}"

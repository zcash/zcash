let
  inherit (import ./config.nix) zcash;
  version = import ../version.nix;
in
  subname: "${zcash.pname}-${version.string}.${subname}"

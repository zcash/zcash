let
  inherit (import ../util) mkLinkDerivation;
in mkLinkDerivation "meta" [
  (import ./impgraph.nix)
  (import ./dependsComparison.nix)
]

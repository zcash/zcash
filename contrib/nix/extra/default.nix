let
  inherit (import ../util) mkLinkDerivation;
in mkLinkDerivation "extra" [
  (import ./impgraph.nix)
  (import ./dependsComparison.nix)
  (import ./tour)
]

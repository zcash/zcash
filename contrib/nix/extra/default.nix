let
  inherit (import ../util) mkLinkDerivation;
in mkLinkDerivation "extra" [
  (import ./debianBinaries.nix)
  (import ./dependsComparison.nix)
  (import ./impgraph.nix)
  (import ./tour)
]

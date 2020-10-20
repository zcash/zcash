let
  source = import ./nixpkgsSource.nix;
in
  import source {}

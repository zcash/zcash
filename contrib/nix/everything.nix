{
  allowInconsistency ? false
}:
  let
    inherit (import ./util) mkLinkDerivation;
  in
    mkLinkDerivation "everything" [
      (import ./zcash.nix { inherit allowInconsistency; })
      (import ./sources)
      (import ./extra)
      ((import ./version.nix).derivation)
    ]

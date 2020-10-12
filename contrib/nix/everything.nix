{
  allowInconsistency ? false
}:
{
  zcash = import ./zcash.nix { inherit allowInconsistency; };
  sources = import ./sources;
  meta = import ./meta;
  version = (import ./version.nix).derivation;
}

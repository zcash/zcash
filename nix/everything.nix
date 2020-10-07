{
  allowInconsistency ? false
}:
{
  zcash = import ./zcash.nix { inherit allowInconsistency; };
  zcashSources = import ./sources;
  zcashMeta = import ./meta;
}

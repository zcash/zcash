let
  inherit (import ./../../../util) nixpkgs;
  inherit (nixpkgs) lib;
  fetchurlWithFallback = import ./../fetchurlWithFallback.nix;
  hardcodedSource = "registry+https://github.com/rust-lang/crates.io-index";
in
  { name, version, checksum, source, dependencies ? null }:
    # Caution: dependencies are ignored!

    assert (source == hardcodedSource);

    # FIXME: This endpoint is potentially unstable.
    # Ref: https://github.com/rust-lang/crates.io/issues/65
    fetchurlWithFallback {
      name = "${name}-${version}.crate";
      url = "https://crates.io/api/v1/crates/${name}/${version}/download";
      sha256 = checksum;
    }

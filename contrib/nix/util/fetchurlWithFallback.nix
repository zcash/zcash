let
  inherit (import ../util) nixpkgs config;
  inherit (nixpkgs) fetchurl;
  inherit (config) zcash;
in
  { archive, url, sha256 }:
    fetchurl {
      inherit sha256;
      name = archive;
      urls = [
        url
        "${zcash.fallbackUrl}/${archive}"
      ];
    }

let
  inherit (import ./nixpkgs.nix) fetchurl;
  inherit (import ./config.nix) zcash;
in
  { url, sha256, name ? null }:
    let
      tarname = 
        let
          inherit (builtins) head match;
        in
          head (match "^.*/([^/]*)$" url);
    in fetchurl {
      inherit sha256;

      ${if name == null then null else "name"} = name;

      urls = [
        # url
        "${zcash.fallbackUrl}/${tarname}"
      ];
    }

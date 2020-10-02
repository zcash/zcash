let
  fetchDepSrcUrl = import ./fetchDepSrcUrl.nix;
in
  { pname, version, source, sha256, url ? null, ... }:
    fetchDepSrcUrl {
      inherit sha256;

      url = builtins.getAttr source {
        "url" = (
          assert url != null;
          url
        );
        "base url" = (
          assert url != null;
          "${url}/${pname}-${version}.tar.gz"
        );
        "github" = (
          assert url == null;
          "https://github.com/${pname}/libevent/archive/${pname}-${version}.tar.gz"
        );
      };
    } 

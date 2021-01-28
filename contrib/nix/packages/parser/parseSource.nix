let
  inherit (import ../../util) nixpkgs;
  inherit (nixpkgs.lib.strings) hasSuffix;
in
  pname: args @ {
    version,
    sha256,
    url,
    ...
  }:
    let
      archive = "${pname}-${version}.tar.gz";
      fullUrl =
        if hasSuffix "/" url
        then "${url}${archive}"
        else url;

    in args // {
      inherit archive pname;
      url = fullUrl;
    }

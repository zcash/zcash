let
  inherit (builtins)
    mapAttrs;
  inherit (import ../util)
    nixpkgs fetchurlWithFallback flip parsedPackages;
  inherit (nixpkgs)
    stdenv;

  mkZcashDerivation =
    name: args @ {
      pname,
      version,
      url,
      sha256,
      source ? "url",
      archive ? null,
      urlbase ? null,
      nativeBuildInputs ? [],
      patches ? [],
      configureFlags ? [],
      makeFlags ? [],
      extraEnv ? {},
      buildscript ? false,
    }:
      assert name == pname;
      let
        pkgPatchDir = ../../depends/patches + "/${pname}";
      in
        stdenv.mkDerivation (
          extraEnv //
          (removeAttrs args ["extraEnv"]) // 
          {
            src = fetchurlWithFallback { inherit url sha256; };
            patches = map (p: "${pkgPatchDir}/${p}") patches;
            nativeBuildInputs = map (flip builtins.getAttr nixpkgs) nativeBuildInputs;
            ${if buildscript then "builder" else null} = ./. + "/${pname}.sh";
          }
        );
in
  mapAttrs mkZcashDerivation parsedPackages

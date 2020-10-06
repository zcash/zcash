let
  inherit (builtins)
    mapAttrs;
  inherit (import ../util)
    nixpkgs fetchurlWithFallback flip;
  inherit (nixpkgs)
    stdenv;

  parsedPackages = import ./parser;

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
    }:
      assert name == pname;
      let
        pkgPatchDir = ../../depends/patches + "/${pname}";
        buildScriptPath = ./. + "/${name}.sh";
        hasBuilder = builtins.pathExists buildScriptPath;
      in
        stdenv.mkDerivation (
          extraEnv //
          (removeAttrs args ["extraEnv"]) // 
          {
            src = fetchurlWithFallback { inherit url sha256; };
            patches = map (p: "${pkgPatchDir}/${p}") patches;
            nativeBuildInputs = map (flip builtins.getAttr nixpkgs) nativeBuildInputs;
            ${if hasBuilder then "builder" else null} = buildScriptPath;
          }
        );
in
  mapAttrs mkZcashDerivation parsedPackages

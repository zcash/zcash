{
  allowInconsistency ? false
}:
let
  inherit (builtins)
    mapAttrs;
  inherit (import ../util)
    nixpkgs fetchurlWithFallback flip zcstdenv;

  parsedPackages = import ./parser { inherit allowInconsistency; };

  mkZcashDerivation =
    name: args @ {
      pname,
      version,
      url,
      archive,
      sha256,
      nativeBuildInputs ? [],
      patches ? [],
      configureFlags ? [],
      makeFlags ? [],
      extraEnv ? {},
    }:
      assert name == pname;
      let
        pkgPatchDir = ../../../depends/patches + "/${pname}";
        buildScriptPath = ./. + "/${name}.sh";
        hasBuilder = builtins.pathExists buildScriptPath;
      in
        zcstdenv.mkDerivation (
          extraEnv //
          (removeAttrs args ["extraEnv"]) //
          {
            src = fetchurlWithFallback { inherit archive url sha256; };
            patches = map (p: "${pkgPatchDir}/${p}") patches;
            nativeBuildInputs = map (flip builtins.getAttr nixpkgs) nativeBuildInputs;
            ${if hasBuilder then "builder" else null} = buildScriptPath;
          }
        );
in
  mapAttrs mkZcashDerivation parsedPackages

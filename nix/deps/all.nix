let
  inherit (builtins)
    mapAttrs;
  inherit (import ../util)
    nixpkgs flip patchDir parsedPackages;
  inherit (nixpkgs)
    stdenv;

  mkZcashDerivation =
    let
      inherit (builtins) getAttr;
      sources = (import ./../sources);
    in
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
        stdenv.mkDerivation (
          extraEnv //
          (removeAttrs args ["extraEnv"]) // 
          {
            src = "${sources}/${archive}";
            patches = map (p: "${patchDir}/${pname}/${p}") patches;
            nativeBuildInputs = map (flip getAttr nixpkgs) nativeBuildInputs;
            ${if buildscript then "builder" else null} = ../packages + "/${pname}.sh";
          }
        );
in
  mapAttrs mkZcashDerivation parsedPackages

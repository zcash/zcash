let
  inherit (builtins)
    mapAttrs;
  inherit (import ../util)
    nixpkgs config flip importTOML patchDir readDirBySuffix;
  inherit (nixpkgs)
    stdenv lib;

  mkZcashDerivation =
    let
      inherit (builtins) getAttr;
      sources = (import ./../sources);
    in
      pname: tomlArgs @ {
        version,
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
        let
          baseArgs = extraEnv // (removeAttrs tomlArgs ["extraEnv"]);
          derivArgs = baseArgs // {
            inherit pname;
            src = "${sources}/${archive}";
            patches = map (p: "${patchDir}/${pname}/${p}") patches;
            nativeBuildInputs = map (flip getAttr nixpkgs) nativeBuildInputs;
            ${if buildscript then "builder" else null} = ./. + "/${pname}.sh";
          };
        in
          stdenv.mkDerivation derivArgs;

  tomls = mapAttrs (_: importToml) (readDirBySuffix ".toml" ./.);
in
  mapAttrs mkZcashDerivation tomls

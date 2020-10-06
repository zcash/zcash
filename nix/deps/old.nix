let
  inherit (builtins) getAttr;
  inherit (import ./../util) nixpkgs config flip patchDir;
  inherit (nixpkgs) stdenv lib;
  inherit (lib) attrsets;
  sources = (import ./../sources);

  dep2derivation = {
    # Note: The full schema is parsed in config.nix.
    pname,
    archive,
    nativeBuildInputs ? [],
    patches ? [],
    buildscript ? false,
    extraEnv ? {},
    ...
  } @ allArgs:
    let
      baseArgs = extraEnv // (removeAttrs allArgs ["extraEnv"]);
      derivArgs = baseArgs // {
        src = "${sources}/${archive}";
        patches = map (p: "${patchDir}/${pname}/${p}") patches;
        nativeBuildInputs = map (flip getAttr nixpkgs) nativeBuildInputs;
        ${if buildscript then "builder" else null} = ./../builders + "/${pname}.sh";
      };
    in
      stdenv.mkDerivation derivArgs;

  dep2derivNV = { pname, ... } @ args:
    { name = pname; value = dep2derivation args; };
in
  builtins.listToAttrs (map dep2derivNV config.dependencies)

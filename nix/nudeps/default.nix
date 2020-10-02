let
  inherit (import ./../util) nixpkgs config patchDir;
  inherit (nixpkgs) stdenv lib;
  inherit (lib) attrsets;
  sources = (import ./../sources);

  dep2derivation = {
    # Note: The full schema is parsed in config.nix.
    pname,
    archive,
    patches ? [],
    buildscript ? false,
    ...
  } @ allArgs:
    let
      derivArgs = allArgs // {
        src = "${sources}/${archive}";
        patches = map (p: "${patchDir}/${pname}/${p}") patches;
        nativeBuildInputs = [
          nixpkgs.autoreconfHook
        ];
        ${if buildscript then "builder" else null} = ./../builders + "/${pname}.sh";
      };
    in
      stdenv.mkDerivation derivArgs;

  dep2derivNV = { pname, ... } @ args:
    { name = pname; value = dep2derivation args; };
in
  builtins.listToAttrs (map dep2derivNV config.dependencies)

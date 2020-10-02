let
  inherit (import ./../util) nixpkgs config patchDir;
  inherit (nixpkgs) stdenv lib;
  inherit (lib) attrsets;
  sources = (import ./../sources);

  dep2derivation = {
    pname,
    version,
    sha256,
    source ? "url",
    url ? null,
    patches ? [],
    configureOptions ? [],
    buildscript ? false,
  } @ allArgs:
    let
      baseDerivArgs = removeAttrs allArgs [
        # Processed by sources/default.nix:
        "source"
        "url"
        "sha256"

        # Overridden:
        "patches"
      ];

      derivArgs = baseDerivArgs // {
        src = "${sources}/${pname}-${version}.tar.gz";
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

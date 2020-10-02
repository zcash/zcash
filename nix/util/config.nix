let
  inherit (import ./nixpkgs.nix) lib;
  inherit (lib.attrsets) mapAttrsToList;

  # Pattern matching only works for function calls, so we define a set
  # of functions to do schema checks:
  parseTOML = { zcash, nixpkgs, dependencies, sources }:
    {
      zcash = parseZcash zcash;
      nixpkgs = parseNixPkgs nixpkgs;
      dependencies = mapAttrsToList parseDependency dependencies;
      sources = map parseSource sources;
    };

  # The {} @ x: x idiom returns the set unchanged if it matches the pattern:
  parseZcash = { pname, version, fallbackUrl } @ good: good;
  parseNixPkgs = { gitrev, sha256 } @ good: good;
  parseSource = { url, sha256 } @ good: good;

  # This one normalizes dependency entries substantially:
  parseDependency = pname: {
    version,
    sha256,
    source ? "url",
    archive ? null,
    urlbase ? null,
    patches ? [],
    configureFlags ? [],
    makeFlags ? [],
    buildscript ? false,
  } @ args:
    assert source == "github" || source == "url";
    assert source == "github" -> (archive == null && urlbase == null);
    assert source == "url" -> urlbase != null;
    let
      _archive =
        if source == "github" || archive == null
        then "${pname}-${version}.tar.gz"
        else archive;

      _urlbase =
        if source == "github"
        then "https://github.com/${pname}/archive/"
        else urlbase;

      url =
        assert _archive != null;
        assert _urlbase != null;
        "${_urlbase}/${_archive}";
    in args // {
      inherit pname url;
      archive = _archive;
      urlbase = _urlbase;
    };

  importTOML = import ./importTOML.nix;
  rawTOML = importTOML ./../config.toml;
in
  parseTOML rawTOML

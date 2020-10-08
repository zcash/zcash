pname: args @ {
  version,
  sha256,
  source ? "url",
  githubOrg ? null,
  archive ? null,
  urlbase ? null,
  ...
}:
assert source == "github" || source == "url";
assert source == "github" -> urlbase == null;
assert source == "url" -> urlbase != null;
assert source == "url" -> githubOrg == null;
let
  default = v: d: if v == null then d else v;

  _archive = default archive "${pname}-${version}.tar.gz";
  _urlbase = default urlbase (
    "https://github.com/${default githubOrg pname}/${pname}/archive"
  );

  url =
    assert _archive != null;
    assert _urlbase != null;
    "${_urlbase}/${_archive}";
in args // {
  inherit pname url;
  archive = _archive;
  urlbase = _urlbase;
}

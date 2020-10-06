let
  foo = "unused";
in
  pname: args @ {
    version,
    sha256,
    source ? "url",
    archive ? null,
    urlbase ? null,
    ...
  }:
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
  }



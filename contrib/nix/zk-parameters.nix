{ cacert
, flock
, stdenv
, wget
}:

stdenv.mkDerivation {
  name = "zk-parameters";

  src = ../..;

  nativeBuildInputs = [
    cacert
    flock
    wget
  ];

  phases = [ "unpackPhase" "patchPhase" "installPhase" ];

  # This is currently needed because it’s the easiest way to break a
  # catch-22. If the build is sandboxed (the ideal), then we can’t download
  # arbitrary files. However, if it’s off, then `/tmp` doesn’t get relocated,
  # and so the nix build user can’t access it. Ideally, we’d be able to figure
  # out how to disable the sandbox _just enough_ to download data at this one
  # point. then this change could go away.
  patches = [
    ./patches/fetch-params.patch
  ];

  # We override `HOME` here because Nix sets it to somewhere unwritable when
  # we’re sandboxed (as we should be). But fetch-params relies on `HOME`. So
  # make a temporary `HOME` until we fix that dependency.
  installPhase = ''
    HOME="$out/var/cache" ./zcutil/fetch-params.sh
  '';
}

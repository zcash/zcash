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

  phases = [ "unpackPhase" "installPhase" ];

  # We override `HOME` here because Nix sets it to somewhere unwritable when
  # we’re sandboxed (as we should be). But fetch-params relies on `HOME`. So
  # make a temporary `HOME` until we fix that dependency.
  installPhase = ''
    HOME="$out/var/cache" $src/zcutil/fetch-params.sh
  '';

  outputHash = "sha256-tRiqwvDDYFS1K41p0BSlrO+2wIdZWI7LWLDVzqDp3vs=";
  outputHashAlgo = "sha256";
  outputHashMode = "recursive";
}

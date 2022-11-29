{ cacert
, flock
, lib
, runCommand
, src
, stdenv
, curl
}:

runCommand "zk-parameters" {
  src = lib.cleanSourceWith {
    inherit src;
    filter = path: type: lib.hasInfix "/zcutil" (toString path);
  };

  nativeBuildInputs = [
    cacert
    flock
    curl
  ];

  outputHash = "sha256-ElJg8gLiHvwgFlUXADzSlYheEjxQcWWKNoVC4+d/Ss4=";
  outputHashAlgo = "sha256";
  outputHashMode = "recursive";
}
  # We override `HOME` here because Nix sets it to somewhere unwritable when
  # we’re sandboxed (as we should be). But fetch-params relies on `HOME`. So
  # make a temporary `HOME` until we fix that dependency.
  ''
    HOME="$out/var/cache" $src/zcutil/fetch-params.sh
  ''

{ cacert
, flock
, lib
, runCommand
, src
, stdenv
, wget
}:

runCommand "zk-parameters" {
  src = lib.cleanSourceWith {
    inherit src;
    filter = path: type: lib.hasInfix "/zcutil" (toString path);
  };

  nativeBuildInputs = [
    cacert
    flock
    wget
  ];

  outputHash = "sha256-jLmX8/YqHKU2NXBj1VMEhHiwgneY2YVqS6qUFlwHYQI=";
  outputHashAlgo = "sha256";
  outputHashMode = "recursive";
}
  # We override `HOME` here because Nix sets it to somewhere unwritable when
  # we’re sandboxed (as we should be). But fetch-params relies on `HOME`. So
  # make a temporary `HOME` until we fix that dependency.
  ''
    HOME="$out/var/cache" $src/zcutil/fetch-params.sh
  ''

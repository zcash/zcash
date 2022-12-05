{
  cacert,
  curl,
  flock,
  lib,
  runCommand,
  src,
  stdenv,
}:
runCommand "zk-parameters" {
  src = lib.cleanSourceWith {
    inherit src;
    filter = path: type: lib.hasInfix "/zcutil" (toString path);
  };

  nativeBuildInputs = [
    cacert
    curl
    flock
  ];

  outputHash = "sha256-jGYbSF0W0KxShMQOMMCpezEw0ERGm1eJdtpXn6Cu6LI=";
  outputHashAlgo = "sha256";
  outputHashMode = "recursive";
}
# We override `HOME` here because Nix sets it to somewhere unwritable when
# we’re sandboxed (as we should be). But fetch-params relies on `HOME`. So
# set `HOME` to the static data directory until we fix that dependency.
''
  HOME="$out/share/zcash" $src/zcutil/fetch-params.sh
''

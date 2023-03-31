{
  cacert,
  curl,
  flock,
  lib,
  runCommand,
  src,
  stdenv,
}:
stdenv.mkDerivation {
  pname = "zk-parameters";
  version = "unknown";

  src = lib.cleanSourceWith {
    inherit src;
    filter = path: type: lib.hasInfix "/zcutil" (toString path);
  };

  patchPhase = ''
    patchShebangs ./zcutil/fetch-params.sh
  '';

  nativeBuildInputs = [
    cacert
    curl
    flock
  ];

  # We override `HOME` here because Nix sets it to somewhere unwritable when
  # we’re sandboxed (as we should be). But fetch-params relies on `HOME`. So
  # set `HOME` to the static data directory until we fix that dependency.
  buildPhase = ''
    HOME="$PWD" ./zcutil/fetch-params.sh
  '';

  installPhase = ''
    mkdir -p $out/share/zcash
    cp -R ./.zcash-params $out/share/zcash/
  '';

  outputHash = "sha256-jGYbSF0W0KxShMQOMMCpezEw0ERGm1eJdtpXn6Cu6LI=";
  outputHashAlgo = "sha256";
  outputHashMode = "recursive";
}

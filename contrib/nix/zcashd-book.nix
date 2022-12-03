{
  mdbook,
  src,
  stdenv,
}:
stdenv.mkDerivation {
  inherit src;

  name = "zcashd-book";

  nativeBuildInputs = [mdbook];

  buildPhase = ''
    runHook preBuild
    mdbook build doc/book
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/share/doc/zcashd
    cp -r doc/book/book $out/share/doc/zcashd
    runHook postInstall
  '';
}

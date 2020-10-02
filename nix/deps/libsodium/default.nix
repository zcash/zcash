let
  inherit (import ./../../util) nixpkgs patchDir;
  inherit (nixpkgs) stdenv;
  sources = import ./../../sources;
in
  stdenv.mkDerivation rec {
    pname = "libsodium";
    version = "1.0.18";
    src = "${sources}/${pname}-${version}.tar.gz";

    patches = [
      "${patchDir}/${pname}/1.0.15-pubkey-validation.diff"
      "${patchDir}/${pname}/1.0.15-signature-validation.diff"
    ];

    configureOptions = [
      "--enable-static"
      "--disable-shared"
    ];

    nativeBuildInputs = [
    ];
  }


with import ./../../../pkgs-pinned.nix;
with import ./../utils.nix;
stdenv.mkDerivation rec {
  pname = "libsodium";
  version = "1.0.18";
  src = fetchurl {
    url = "https://download.libsodium.org/libsodium/releases/${pname}-${version}.tar.gz";
    sha256 = "6f504490b342a4f8a4c4a02fc9b866cbef8622d5df4e5452b46be121e46636c1";
  };

  patches = [
    ./../../../../../depends/patches/libsodium/1.0.15-pubkey-validation.diff
    ./../../../../../depends/patches/libsodium/1.0.15-signature-validation.diff
  ];

  configureOptions = [
    "--enable-static"
    "--disable-shared"
  ];

  nativeBuildInputs = [
  ];
}


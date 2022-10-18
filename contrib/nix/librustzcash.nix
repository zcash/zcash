{ crane
, darwin
, lib
, libiconv
, stdenv
}:

crane.buildPackage {
  src = ../..;

  buildInputs = lib.optionals stdenv.isDarwin [
    darwin.apple_sdk.frameworks.Security
    libiconv
  ];

  doCheck = true;
}

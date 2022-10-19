{ crane
, darwin
, lib
, libiconv
, stdenv
}:

crane.buildPackage {
  src = lib.cleanSourceWith {
    filter = path: type:
      crane.filterCargoSources path type
      || lib.hasSuffix ".dat" (baseNameOf (toString path));
    src = ../..;
  };

  buildInputs = lib.optionals stdenv.isDarwin [
    darwin.apple_sdk.frameworks.Security
    libiconv
  ];

  doCheck = true;
}

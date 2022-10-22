{ crane
, darwin
, lib
, libiconv
, src
, stdenv
}:

crane.buildPackage {
  src = lib.cleanSourceWith {
    filter = path: type:
      crane.filterCargoSources path type
      || lib.hasSuffix ".dat" (baseNameOf (toString path));
    src = src;
  };

  buildInputs = lib.optionals stdenv.isDarwin [
    darwin.apple_sdk.frameworks.Security
    libiconv
  ];

  doCheck = true;
}

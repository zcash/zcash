let
  inherit (import ./util) nixpkgs config srcDir;
  inherit (nixpkgs) stdenv;
  zcdeps = import ./deps;
in
  stdenv.mkDerivation {
    inherit (config.zcash) pname version;
    src = srcDir;

    nativeBuildInputs = [
      nixpkgs.autoreconfHook
      nixpkgs.file
      nixpkgs.git
      nixpkgs.hexdump
      nixpkgs.pkg-config
      zcdeps.boost
      zcdeps.vendoredCrates
      zcdeps.bdb
      zcdeps.googletest
      zcdeps.libevent
      zcdeps.libsodium
      zcdeps.native_rust
      zcdeps.openssl
      zcdeps.utfcpp
    ];
  
    CONFIG_SITE = nixpkgs.writeText "config.site" ''
      RUST_TARGET='${nixpkgs.buildPlatform.config}'
      RUST_VENDORED_SOURCES='${zcdeps.vendoredCrates}'
    '';

    configureFlags = [
      "--with-boost=${zcdeps.boost}"
    ];

    # Patch absolute paths from libtool to use nix file:
    # See https://github.com/NixOS/nixpkgs/issues/98440
    preConfigure = ''sed -i 's,/usr/bin/file,${nixpkgs.file}/bin/file,g' ./configure'';
  }

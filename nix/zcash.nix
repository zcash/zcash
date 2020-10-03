let
  inherit (import ./util) nixpkgs config srcDir;
  inherit (nixpkgs) stdenv;
  inherit (config.zcash) pname version;

  zcdeps = import ./deps;
  vendoredCrates = "${import ./sources}/${pname}-${version}-vendored-crates";
in
  stdenv.mkDerivation {
    inherit pname version;
    src = srcDir;

    nativeBuildInputs = [
      nixpkgs.autoreconfHook
      nixpkgs.file
      nixpkgs.git
      nixpkgs.hexdump
      nixpkgs.pkg-config
      zcdeps.bdb
      zcdeps.boost # FIXME: Is this needed here vs configureFlags?
      zcdeps.googletest
      zcdeps.libevent
      zcdeps.libsodium
      zcdeps.native_rust
      zcdeps.openssl
      zcdeps.utfcpp
      vendoredCrates # FIXME: Is this needed here vs CONFIG_SITE?
    ];

    CONFIG_SITE = nixpkgs.writeText "config.site" ''
      RUST_TARGET='${nixpkgs.buildPlatform.config}'
      RUST_VENDORED_SOURCES='${vendoredCrates}'
    '';

    configureFlags = [
      "--with-boost=${zcdeps.boost}"
    ];

    # Patch absolute paths from libtool to use nix file:
    # See https://github.com/NixOS/nixpkgs/issues/98440
    preConfigure = ''sed -i 's,/usr/bin/file,${nixpkgs.file}/bin/file,g' ./configure'';
  }

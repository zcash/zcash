let
  inherit (import ./util) nixpkgs config srcDir;
  inherit (nixpkgs) stdenv;
  inherit (config.zcash) pname version;

  zcdeps = import ./deps;
  zcnudeps = import ./nudeps;
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
      zcnudeps.bdb
      zcnudeps.boost # FIXME: Is this needed here vs configureFlags?
      zcnudeps.libevent
      zcnudeps.libsodium
      zcnudeps.utfcpp
      zcdeps.googletest
      zcdeps.native_rust
      zcdeps.openssl
      vendoredCrates # FIXME: Is this needed here vs CONFIG_SITE?
    ];

    CONFIG_SITE = nixpkgs.writeText "config.site" ''
      RUST_TARGET='${nixpkgs.buildPlatform.config}'
      RUST_VENDORED_SOURCES='${vendoredCrates}'
    '';

    configureFlags = [
      "--with-boost=${zcnudeps.boost}"
    ];

    # Patch absolute paths from libtool to use nix file:
    # See https://github.com/NixOS/nixpkgs/issues/98440
    preConfigure = ''sed -i 's,/usr/bin/file,${nixpkgs.file}/bin/file,g' ./configure'';
  }

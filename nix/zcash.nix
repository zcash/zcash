let
  inherit (import ./util) nixpkgs config srcDir;
  inherit (nixpkgs) stdenv;
  inherit (config.zcash) pname version;

  oldDeps = import ./deps/old.nix;
  deps = import ./deps/all.nix;
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
      deps.bdb
      oldDeps.boost # FIXME: Is this needed here vs configureFlags?
      oldDeps.googletest
      oldDeps.libevent
      oldDeps.libsodium
      oldDeps.native_rust
      oldDeps.openssl
      oldDeps.utfcpp
      vendoredCrates # FIXME: Is this needed here vs CONFIG_SITE?
    ];

    CONFIG_SITE = nixpkgs.writeText "config.site" ''
      RUST_TARGET='${nixpkgs.buildPlatform.config}'
      RUST_VENDORED_SOURCES='${vendoredCrates}'
    '';

    configureFlags = [
      "--with-boost=${oldDeps.boost}"
    ];

    # Patch absolute paths from libtool to use nix file:
    # See https://github.com/NixOS/nixpkgs/issues/98440
    preConfigure = ''sed -i 's,/usr/bin/file,${nixpkgs.file}/bin/file,g' ./configure'';
  }

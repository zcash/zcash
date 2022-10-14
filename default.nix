# In development, load this with `nix-shell`
# In CI, build this with `nix-build --pure`
#
# NB: Do not remove anything from this file without testing it against
#    `nix-build --pure`. This builds it in a sandboxed environment, but makes
#     development much harder as it means most of your not-required-for-build
#     tooling is unavailable.
#
# Currently need to run
# > ./configure $configureFlags
# which I don’t understand, because the boost-libdir should be in LD_FLAGS, etc. already.

{ pkgs ? import ./contrib/nix/pkgs.nix {} }:

pkgs.llvmPackages_14.stdenv.mkDerivation {
  name = "zcash";

  src = ./.;
  
  # These are roughly copied from depends/packages/packages.mk, however they
  # should be more complete, as `nix-build --pure` prevents us from relying on
  # any system libraries or tools.
  
  buildInputs = [
    pkgs.boost
    pkgs.db62
    pkgs.gtest
    pkgs.libevent
    pkgs.openssl
    (pkgs.libsodium.overrideAttrs (old: {
      patches = old.patches ++ [
        ./depends/patches/libsodium/1.0.15-pubkey-validation.diff
        ./depends/patches/libsodium/1.0.15-signature-validation.diff
      ];
    }))
    pkgs.utf8cpp
    (pkgs.zeromq.overrideAttrs (old: {
      patches = [
        ./depends/patches/zeromq/windows-unused-variables.diff
      ];
    }))
  ];

  nativeBuildInputs = [
    pkgs.autoreconfHook
    pkgs.cargo
    pkgs.cxx-rs
    pkgs.git
    pkgs.hexdump
    pkgs.makeWrapper
    pkgs.pkg-config
    pkgs.python3
    pkgs.rustc
  ];

  # I think this is needed because the “utf8cpp” dir component is non-standard,
  # but I don’t know why the package doesn’t set that up correctly.
  CXXFLAGS = "-I${pkgs.utf8cpp}/include/utf8cpp";

  RUST_TARGET = pkgs.buildPlatform.config;

  configureFlags = [
    "--enable-online-rust"
    "--with-boost-libdir=${pkgs.boost}/lib"
  ];
}

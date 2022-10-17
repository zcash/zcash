{
  description = "Internet money";

  outputs = { self, crane, flake-utils, nixpkgs, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
        {
          packages = rec {
            default = zcash;

            librustzcash = crane.lib.${system}.buildPackage {
              src = ./.;

              buildInputs = pkgs.lib.optionals pkgs.stdenv.isDarwin [
                pkgs.darwin.apple_sdk.frameworks.Security
                pkgs.libiconv
              ];

              doCheck = true;
            };

            zcash = pkgs.llvmPackages_14.stdenv.mkDerivation {
              pname = "zcash";
              version = "5.3.0";

              src = ./.;

              buildInputs = [
                librustzcash
                pkgs.boost
                pkgs.db62
                pkgs.gtest
                pkgs.libevent
                (pkgs.libsodium.overrideAttrs (old: {
                  patches = old.patches ++ [
                    ./depends/patches/libsodium/1.0.15-pubkey-validation.diff
                    ./depends/patches/libsodium/1.0.15-signature-validation.diff
                  ];
                }))
                pkgs.openssl
                pkgs.utf8cpp
                (pkgs.zeromq.overrideAttrs (old: {
                  patches = [
                    ./depends/patches/zeromq/windows-unused-variables.diff
                  ];
                }))
              ];

              nativeBuildInputs = [
                pkgs.autoreconfHook
                pkgs.cxx-rs
                pkgs.hexdump
                pkgs.makeWrapper
                pkgs.pkg-config
                pkgs.python3
              ];

              # I think this is needed because the “utf8cpp” dir component is
              # non-standard, but I don’t know why the package doesn’t set that
              # up correctly.
              CXXFLAGS = "-I${pkgs.utf8cpp}/include/utf8cpp";

              configureFlags = [
                "--with-boost-libdir=${pkgs.boost}/lib"
                "--with-rustzcash-dir=${librustzcash}"
              ];
            };
          };
        });

  inputs = {
    crane = {
      inputs.nixpkgs.follows = "nixpkgs";
      url = "github:ipetkov/crane";
    };

    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };

    flake-utils.url = "github:numtide/flake-utils";

    # We use unstable because cxx-rs isn’t in a release yet … but we should
    # probably be able to use 22.11 or whatever once it’s out.
    nixpkgs.url = "nixpkgs/nixpkgs-unstable";
  };
}

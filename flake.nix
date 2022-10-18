{
  description = "Internet money";

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

            zk-parameters = pkgs.stdenv.mkDerivation {
              name = "zk-parameters";

              src = ./.;

              nativeBuildInputs = [
                pkgs.cacert
                pkgs.flock
                pkgs.wget
              ];

              phases = [ "unpackPhase" "patchPhase" "installPhase" ];

              # This is currently needed because it’s the easiest way to break a
              # catch-22. If the build is sandboxed (the ideal), then we can’t
              # download arbitrary files. However, if it’s off, then `/tmp`
              # doesn’t get relocated, and so the nix build user can’t access
              # it. Ideally, we’d be able to figure out how to disable the
              # sandbox _just enough_ to download data at this one point. then
              # this change could go away.
              patches = [
                ./contrib/nix/patches/fetch-params.patch
              ];

              # We override `HOME` here because Nix sets it to somewhere
              # unwritable when we’re sandboxed (as we should be). But
              # fetch-params relies on `HOME`. So make a temporary `HOME` until
              # we fix that dependency.
              installPhase = ''
                HOME="$out/var/cache" ./zcutil/fetch-params.sh
              '';
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
                zk-parameters
              ];

              # I think this is needed because the “utf8cpp” dir component is
              # non-standard, but I don’t know why the package doesn’t set that
              # up correctly.
              CXXFLAGS = "-I${pkgs.utf8cpp}/include/utf8cpp";

              # Because of fetch-params, everything expects the parameters to be
              # in `HOME`.
              HOME = "${zk-parameters}/var/cache";

              configureFlags = [
                "--with-boost-libdir=${pkgs.boost}/lib"
                "--with-rustzcash-dir=${librustzcash}"
              ];

              doCheck = true;
            };
          };
        });
}

{
  description = "Internet money";

  outputs = {
    self,
    crane,
    flake-utils,
    nixpkgs,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
        overlays = [(import ./contrib/nix/dependencies.nix)];
      };

      # Specific derivations may further filter the src, but this cuts out all
      # the general cruft to start with (source control info, Nix build info,
      # “depends/” build info, etc.).
      src = pkgs.lib.cleanSourceWith {
        filter = path: type:
          ! (pkgs.lib.hasSuffix ".nix" path
            || pkgs.lib.hasInfix "/depends" path
            || pkgs.lib.hasInfix "/src/crc32c" path
            || pkgs.lib.hasInfix "/src/crypto/ctaes" path
            || pkgs.lib.hasInfix "/src/leveldb" path
            || pkgs.lib.hasInfix "/src/secp256k1" path
            || pkgs.lib.hasSuffix "/src/tinyformat.h" path
            || pkgs.lib.hasInfix "/src/univalue" path
            || pkgs.lib.hasInfix "/qa/zcash/checksec.sh" path)
          || pkgs.lib.hasInfix "/depends/patches" path;
        src = pkgs.lib.cleanSource ./.;
      };

      callPackage = pkgs.lib.callPackageWith (pkgs // {inherit src;});
      callPackages = pkgs.lib.callPackagesWith (pkgs // {inherit src;});
    in {
      checks = callPackages ./contrib/nix/checks.nix {};

      formatter = pkgs.alejandra;

      packages = {
        default = self.packages.${system}.zcash;

        librustzcash = callPackage ./contrib/nix/librustzcash.nix {
          crane = crane.lib.${system};
        };

        zk-parameters = callPackage ./contrib/nix/zk-parameters.nix {};

        zcash = callPackage ./contrib/nix/zcash.nix {
          inherit (self.packages.${system}) librustzcash zk-parameters;
        };

        zcashd-book = callPackage ./contrib/nix/zcashd-book.nix {};
      };

      apps = {
        default = self.apps.${system}.zcashd;

        bench_bitcoin = {
          type = "app";
          program = "${self.packages.${system}.zcash}/bin/bench_bitcoin";
        };

        zcash-cli = {
          type = "app";
          program = "${self.packages.${system}.zcash}/bin/zcash-cli";
        };

        zcash-inspect = {
          type = "app";
          program = "${self.packages.${system}.zcash}/bin/zcash-inspect";
        };

        zcash-tx = {
          type = "app";
          program = "${self.packages.${system}.zcash}/bin/zcash-tx";
        };

        zcashd = {
          type = "app";
          program = "${self.packages.${system}.zcash}/bin/zcashd";
        };

        zcashd-wallet-tool = {
          type = "app";
          program = "${self.packages.${system}.zcash}/bin/zcashd-wallet-tool";
        };
      };

      devShells =
        {
          default = self.devShells.${system}.zcash;

          librustzcash = self.packages.${system}.librustzcash.overrideAttrs (old: {
            nativeBuildInputs =
              old.nativeBuildInputs
              ++ [
                pkgs.rust-analyzer # LSP server
              ];
          });

          zcash = self.packages.${system}.zcash.overrideAttrs (old: {
            nativeBuildInputs =
              old.nativeBuildInputs
              ++ [
                pkgs.lldb # debugger
                pkgs.valgrind # debugger
              ];
          });
        }
        // (
          # `pkgs.debian-devscripts` is Linux-specific, so we can only do a
          # release from there.
          if pkgs.lib.hasSuffix "-linux" system
          then {
            # FIXME: sec-hard doesn’t currently pass
            # TODO: I don’t think this should be a shell, maybe an app.
            full_test_suite = callPackage ./contrib/nix/full_test_suite.nix {
              inherit src;
              inherit (self.packages.${system}) zcash;

            };

            release = pkgs.mkShell {
              inherit src;

              nativeBuildInputs = [
                pkgs.debian-devscripts
                pkgs.help2man
                (pkgs.python.withPackages (pypkgs: [
                  pypkgs.progressbar2
                  pypkgs.requests
                  pypkgs.xdg
                ]))
              ];
            };
          }
          else {}
        );
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

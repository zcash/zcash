{
  description = "Internet money";

  outputs = {
    self,
    crane,
    flake-utils,
    nixpkgs,
  }:
    {
      overlays.default = final: prev: {
        zcash = self.packages.${final.system}.zcash;
        zcashd-book = self.packages.${final.system}.zcashd-book;
      };
    }
    // flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
        overlays = [
          (import ./contrib/nix/dependencies/system.nix)
          (import ./contrib/nix/dependencies/depends)
          (import ./contrib/nix/dependencies/subtrees)
        ];
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
          default = pkgs.mkShell {
            # This should include all the packages we define, so that a user in
            # the default shell (automatically entered via direnv) has access to
            # all tooling required for any builds or checks.
            inputsFrom =
              builtins.attrValues self.checks.${system}
              ++ builtins.attrValues self.packages.${system};

            # This contains any additional tooling we want as a developer (e.g.,
            # things like debuggers that aren’t used in any build process but
            # that we want around while doing day-to-day work.
            nativeBuildInputs = [
              pkgs.lldb # debugger
              pkgs.rust-analyzer # LSP server
              # pkgs.valgrind # debugger # currently broken?
            ];
          };
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

      checks = callPackages ./contrib/nix/checks.nix {};

      formatter = pkgs.alejandra;
    });

  inputs = {
    crane = {
      inputs.nixpkgs.follows = "nixpkgs";
      url = github:ipetkov/crane;
    };

    flake-utils.url = github:numtide/flake-utils;

    nixpkgs.url = github:NixOS/nixpkgs/release-22.11;
  };
}

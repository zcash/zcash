{
  description = "Internet money";

  outputs = { self, crane, flake-utils, nixpkgs, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [ (import ./contrib/nix/dependencies.nix) ];
        };
      in
        {
          packages = {
            default = self.packages.${system}.zcash;

            librustzcash = pkgs.callPackage ./contrib/nix/librustzcash.nix {
              crane = crane.lib.${system};
            };

            zk-parameters = pkgs.callPackage ./contrib/nix/zk-parameters.nix {
            };

            zcash = pkgs.callPackage ./contrib/nix/zcash.nix {
              inherit (self.packages.${system}) librustzcash zk-parameters;
            };
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
              program =
                "${self.packages.${system}.zcash}/bin/zcashd-wallet-tool";
            };
          };

          devShells = {
            release = pkgs.mkShell {
              nativeBuildInputs = [
                pkgs.debian-devscripts
                pkgs.help2man
                (pkgs.python3.withPackages (pypkgs: [
                  pypkgs.progressbar2
                  pypkgs.requests
                ]))
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

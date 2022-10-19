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
        # This doesn’t quite do the same thing as loading overlays, but it’s
        # close?
        applyOverlays = super:
          super.lib.foldl (self: overlay: self // overlay self super) super;

        pkgs = applyOverlays nixpkgs.legacyPackages.${system} [
          (import ./contrib/nix/dependencies.nix)
        ];

        callPackage = pkgs.lib.callPackageWith pkgs;
      in
        {
          packages = rec {
            default = zcash;

            librustzcash = callPackage ./contrib/nix/librustzcash.nix {
              crane = crane.lib.${system};
            };

            zk-parameters = callPackage ./contrib/nix/zk-parameters.nix {
            };

            zcash = callPackage ./contrib/nix/zcash.nix {
              inherit librustzcash zk-parameters;
            };
          };

          apps = rec {
            default = zcashd;

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
        });
}

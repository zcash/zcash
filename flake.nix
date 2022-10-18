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

            librustzcash = pkgs.callPackage ./contrib/nix/librustzcash.nix {
              crane = crane.lib.${system};
            };

            zk-parameters = pkgs.callPackage ./contrib/nix/zk-parameters.nix {
            };

            zcash = pkgs.callPackage ./contrib/nix/zcash.nix {
              inherit librustzcash zk-parameters;
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

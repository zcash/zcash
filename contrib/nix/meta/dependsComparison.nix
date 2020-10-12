let
  inherit (import ../util) mkInternalDerivation nixpkgs selectSource;

  subname = "nix-depends-comparison";
  buildFrameworks = [
    "depends"
    "contrib/nix"
  ];
in
  mkInternalDerivation {
    inherit buildFrameworks;

    subname = "${subname}.txt";
    src = selectSource "${subname}-source" "." buildFrameworks;

    builder = ''
      source "$stdenv/setup"
      set -euo pipefail

      (
        cd "$src"
        echo '=== depends framework ==='
        wc -l ./depends/{Makefile,config.site.in,*.mk,builders/*,hosts/*}
        echo '=== depends packages ==='
        wc -l ./depends/packages/*

        echo
        echo '=== ./contrib/nix framework ==='
        wc -l $(find ./contrib/nix -type f -name '*.nix')
        echo '=== ./contrib/nix packages ==='
        wc -l ./contrib/nix/packages/*.{toml,sh}
      ) > "$out"
    '';
  }

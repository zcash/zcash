let
  inherit (import ../util) config nixpkgs;
  inherit (nixpkgs) stdenv writeScript;
  inherit (nixpkgs) python3 graphviz;
  inherit (config.zcash) pname;

  version = import ../version.nix;
  name = "${pname}-${version}-nix-import-graphs";
in
  stdenv.mkDerivation {
    inherit name graphviz;
    src = ../.;
    builder = writeScript "${name}-builder.sh" ''
      set -efuxo pipefail
      exec ${python3}/bin/python ${./impgraph.py}
    '';

    # Ref: https://github.com/NixOS/nixpkgs/blob/master/pkgs/development/libraries/pipewire/default.nix
    FONTCONFIG_FILE = nixpkgs.makeFontsConf {
      fontDirectories = [];
    };
  }

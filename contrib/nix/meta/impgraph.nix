let
  inherit (import ../util) mkInternalDerivation nixpkgs;
  inherit (nixpkgs) python3 graphviz;

in
  mkInternalDerivation {
    inherit graphviz;

    subname = "nix-import-graphs";
    src = ./..;
    builder = ''
      set -efuxo pipefail
      exec ${python3}/bin/python ${./impgraph.py}
    '';

    # Ref: https://github.com/NixOS/nixpkgs/blob/master/pkgs/development/libraries/pipewire/default.nix
    FONTCONFIG_FILE = nixpkgs.makeFontsConf {
      fontDirectories = [];
    };
  }

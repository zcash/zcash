let
  inherit (import ../util) idevName nixpkgs;
  inherit (nixpkgs) stdenv writeScript;
  inherit (nixpkgs) python3 graphviz;

  idevDesc = "nix-import-graphs";
in
  stdenv.mkDerivation {
    inherit graphviz;

    name = idevName idevDesc;
    src = ../.;
    builder = writeScript (idevName (idevDesc + ".sh")) ''
      set -efuxo pipefail
      exec ${python3}/bin/python ${./impgraph.py}
    '';

    # Ref: https://github.com/NixOS/nixpkgs/blob/master/pkgs/development/libraries/pipewire/default.nix
    FONTCONFIG_FILE = nixpkgs.makeFontsConf {
      fontDirectories = [];
    };
  }

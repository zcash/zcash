let
  inherit (import ../util) idevName nixpkgs zcstdenv;
  inherit (nixpkgs) python3 graphviz writeScript;

  idevDesc = "nix-import-graphs";
in
  zcstdenv.mkDerivation {
    inherit graphviz;

    name = idevName idevDesc;
    src = ./..;
    builder = writeScript (idevName (idevDesc + ".sh")) ''
      set -efuxo pipefail
      exec ${python3}/bin/python ${./impgraph.py}
    '';

    # Ref: https://github.com/NixOS/nixpkgs/blob/master/pkgs/development/libraries/pipewire/default.nix
    FONTCONFIG_FILE = nixpkgs.makeFontsConf {
      fontDirectories = [];
    };
  }

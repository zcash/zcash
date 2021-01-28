# This derives the version string into a file:
let
  inherit (import ./util) nixpkgs config zcstdenv;
  inherit (nixpkgs) writeScript;
  inherit (config.zcash) pname;
in rec {
  # The attr names are written for . attr lookup: ie: version.derivation; version.string

  derivation = zcstdenv.mkDerivation {
    name = "${pname}-version";
    src = ../..;

    nativeBuildInputs = [
      nixpkgs.git
    ];

    builder = writeScript "calc-${pname}-version.sh" ''
      source "$stdenv/setup"
      cd "$src"
      git describe --dirty > "$out"
    '';
  };

  string =
    let
      inherit (builtins) head length;
      inherit (nixpkgs.lib.strings) fileContents splitString;
      raw = fileContents derivation;
      parts = splitString "-" raw;
      versionTag = head parts;
      suffix =
        if length parts > 1
        then "-dev"
        else "";
    in
      versionTag + suffix;
}

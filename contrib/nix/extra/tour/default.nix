let
  inherit (import ../../util) nixpkgs mkInternalDerivation;
  inherit (nixpkgs) fetchzip pandoc unzip;

  subname = "nix-tour";

in mkInternalDerivation {
  inherit subname;

  src = ./nix-tour.md;
  nativeBuildInputs = [ pandoc unzip ];
  s5 = fetchzip {
    url = "https://meyerweb.com/eric/tools/s5/v/1.1/s5-11.zip";
    sha256 = "sha256:01ihcldn1abzsqdbjl386kqvd8d080hmi3lkqv18947cwcdbnff9";
    stripRoot = false;
  };
  patch = ./pretty.patch;
  builder = ./builder.sh;
}

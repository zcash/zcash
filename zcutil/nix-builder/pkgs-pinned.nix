let
  # channelrev taken from https://status.nixos.org/ for nixos-20.09
  rev = "61525137fd1002f6f2a5eb0ea27d480713362cd5";
  sha256 = "1gzwz9jvfcf0is6zma7qlgszkngfb2aa4kam0nhs0qnwb4nqn7mg";

  url = "https://github.com/NixOS/nixpkgs-channels/archive/${rev}.tar.gz";
in
  import (fetchTarball { inherit url sha256; }) {}

let
  # channelrev taken from https://status.nixos.org/ for nixos-20.09
  channelrev = "61525137fd1002f6f2a5eb0ea27d480713362cd5";

  channelurl = "https://github.com/NixOS/nixpkgs-channels/archive/${channelrev}.tar.gz";
in
  import (fetchTarball channelurl) {}

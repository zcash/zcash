let
  pkgs = import ./../../../pkgs-pinned.nix;
in
  with pkgs;
  stdenv.mkDerivation rec {
    pname = "libevent";
    version = "2.1.8";
    src = fetchurl {
      url = "https://github.com/libevent/libevent/archive/${pname}-${version}.tar.gz";
      sha256 = "316ddb401745ac5d222d7c529ef1eada12f58f6376a66c1118eee803cb70f83d";
    };
    
    configureOptions = [
      "--disable-shared"
      "--disable-openssl"
      "--disable-libevent-regress"
    ] ++ (
      # Platform/Architecture specific opts:
      let
        # FIXME: don't hardcode platform/arch:
        platform = "linux";
        release = true;

        releaseOpts = ["--disable-debug-mode"];
        platformOpts = {
          linux = ["--with-pic"];
          freebsd = ["--with-pic"];
        };
      in
        platformOpts.${platform} ++ (if release then releaseOpts else [])
    );

    patches = ./patches;
    builder = ./builder.sh;
    nativeBuildInputs = [
      autoreconfHook
    ];
  }


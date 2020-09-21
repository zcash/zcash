with import ./../../../pkgs-pinned.nix;
with import ./../utils.nix;
stdenv.mkDerivation rec {
  pname = "libevent";
  version = "2.1.8";
  src = fetchurl {
    url = "https://github.com/libevent/libevent/archive/${pname}-${version}.tar.gz";
    sha256 = "316ddb401745ac5d222d7c529ef1eada12f58f6376a66c1118eee803cb70f83d";
  };

  configureOptions = selectByHostPlatform [
    [
      "--disable-shared"
      "--disable-openssl"
      "--disable-libevent-regress"
    ]
    {
      kernel = "linux";
      values = ["--with-pic"];
    }
    {
      kernel = "freebsd";
      values = ["--with-pic"];
    }
    [
      # FIXME: handle removing this hardcoding to match ./depends?
      "--disable-debug-mode"
    ]
  ];

  patches = ./patches;
  builder = ./builder.sh;
  nativeBuildInputs = [
    autoreconfHook
  ];
}


let
  inherit (import ./../../util) nixpkgs requirePlatform patchDir;
  inherit (nixpkgs) stdenv fetchurl;
in
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
      (requirePlatform "[^-]*-[^-]*-linux-gnu" "--with-pic")
      # FIXME: handle removing this hardcoding to match ./depends?
      "--disable-debug-mode"
    ];

    patches = [
      "${patchDir}/${pname}/detect-arch4random_addrandom.patch"
      "${patchDir}/${pname}/detect-arch4random_addrandom-fix.patch"
    ];

    builder = ./builder.sh;
    nativeBuildInputs = [
      nixpkgs.autoreconfHook
    ];
  }


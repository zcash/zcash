let
  inherit (import ./../../util) nixpkgs requirePlatform patchDir;
  inherit (nixpkgs) stdenv;
  sources = import ./../../sources;
in
  stdenv.mkDerivation rec {
    pname = "libevent";
    version = "2.1.8";
    src = "${sources}/${pname}-${version}.tar.gz";

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


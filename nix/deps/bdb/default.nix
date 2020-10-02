let
  inherit (import ./../../util) nixpkgs requirePlatform;
  inherit (nixpkgs) stdenv;
  sources = import ./../../sources;
in
  stdenv.mkDerivation rec {
    pname = "bdb";
    version = "6.2.23";
    src = "${sources}/db-${version}.tar.gz";

    configureOptions = [
      "--disable-shared"
      "--enable-cxx"
      "--disable-replication"
      (requirePlatform "[^-]*-[^-]*-linux-gnu" "--with-pic")
    ];

    configureCxxflags = "-std=c++11";
    makeTargets = "libdb_cxx-6.2.a libdb-6.2.a";

    builder = ./builder.sh;
    nativeBuildInputs = [
      nixpkgs.autoreconfHook
    ];
  }


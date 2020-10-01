let
  inherit (import ./../../util) nixpkgs requirePlatform fetchDepSrc;
  inherit (nixpkgs) stdenv;
in
  stdenv.mkDerivation rec {
    pname = "bdb";
    version = "6.2.23";
    src = fetchDepSrc {
      url = "https://download.oracle.com/berkeley-db/db-${version}.tar.gz";
      sha256 = "47612c8991aa9ac2f6be721267c8d3cdccf5ac83105df8e50809daea24e95dc7";
    };

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


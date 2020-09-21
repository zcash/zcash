let
  pkgs = import ./../../../pkgs-pinned.nix;
in
  with pkgs;
  stdenv.mkDerivation rec {
    pname = "bdb";
    version = "6.2.23";
    src = fetchurl {
      url = "https://download.oracle.com/berkeley-db/db-${version}.tar.gz";
      sha256 = "47612c8991aa9ac2f6be721267c8d3cdccf5ac83105df8e50809daea24e95dc7";
    };
    
    configureOptions = [
      "--disable-shared"
      "--enable-cxx"
      "--disable-replication"
    ] ++ [(
      let platform_opts = {
        mingw32 = "--enable-mingw";
        linux = "--with-pic";
        freebsd = "--with-pic";
        darwin = "--disable-atomicsupport";
        aarch64 = "--disable-atomicsupport";
      };
      in
        # FIXME: figure out the nix way to query the platform:
        platform_opts.linux
    )];
    configureCxxflags = "-std=c++11";
    makeTargets = "libdb_cxx-6.2.a libdb-6.2.a";

    builder = ./builder.sh;
    nativeBuildInputs = [
      autoreconfHook
    ];
  }


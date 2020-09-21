with import ./../../../pkgs-pinned.nix;
with import ./../utils.nix;
stdenv.mkDerivation rec {
  pname = "bdb";
  version = "6.2.23";
  src = fetchurl {
    url = "https://download.oracle.com/berkeley-db/db-${version}.tar.gz";
    sha256 = "47612c8991aa9ac2f6be721267c8d3cdccf5ac83105df8e50809daea24e95dc7";
  };

  configureOptions = selectByHostPlatform [
    [ # All hosts:
      "--disable-shared"
      "--enable-cxx"
      "--disable-replication"
    ]
    # FIXME: mingw32 support dropped in this branch. :-/
    # mingw32 = "--enable-mingw";
    {
      kernel = "linux";
      values = ["--with-pic"];
    }
    {
      kernel = "freebsd";
      values = ["--with-pic"];
    }
    {
      kernel = "freebsd";
      values = ["--disable-atomicsupport"];
    }
    {
      # FIXME: Is this really a CPU type?
      cpu = "aarch64";
      values = ["--disable-atomicsupport"];
    }
  ];

  configureCxxflags = "-std=c++11";
  makeTargets = "libdb_cxx-6.2.a libdb-6.2.a";

  builder = ./builder.sh;
  nativeBuildInputs = [
    autoreconfHook
  ];
}


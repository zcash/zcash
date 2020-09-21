with import ./../../../pkgs-pinned.nix;
with import ./../utils.nix;
stdenv.mkDerivation rec {
  pname = "boost";
  version = "1.70.0";

  # The underscored version used in filenames and elsewhere:
  version_ =
    with lib.strings;
    concatStringsSep "_" (splitString "." version);

  src = fetchurl {
    url = "https://dl.bintray.com/boostorg/release/${version}/source/${pname}_${version_}.tar.bz2";
    sha256 = "430ae8354789de4fd19ee52f3b1f739e1fba576f0aded0897c3c2bc00fb38778";
  };

  boostlibs = "chrono,filesystem,program_options,system,thread,test";
  configureOptions = selectByHostPlatform [
    [
      "variant=release" # FIXME: un-hardcode variant?
      "--layout=system"
      "threading=multi"
      "link=static"
      "-sNO_BZIP2=1"
      "-sNO_ZLIB=1"
    ]
    {
      kernel = "linux";
      values = [
        "threadapi=pthread"
        "runtime-link=shared"
      ];
    }
    {
      kernel = "linux";
      cpu = "i686";
      values = [
        "address-model=32"
        "architecture=x86"
      ];
    }
    {
      # FIXME: Is darwin a kernel?
      kernel = "darwin";
      values = [
        "--toolset=darwin-4.2.1"
        "runtime-link=shared"
      ];
    }
    #{
    #  # FIXME: We don't know what mingw is.
    #  ??? = "mingw32";
    #  values = [
    #    "binary-format=pe"
    #    "target-os=windows"
    #    "threadapi=win32"
    #    "runtime-link=static"
    #  ];
    #}
    #{
    #  # FIXME: We don't know what mingw is.
    #  ??? = "mingw32";
    #  cpu = "x86_64";
    #  values = ["address-model=64"];
    #}
    #{
    #  # FIXME: We don't know what mingw is.
    #  ??? = "mingw32";
    #  cpu = "i686";
    #  values = ["address-model=32"];
    #}
  ];

  cxxflags = selectByHostPlatform [
    [
      "-std=c++11"
      "-fvisibility=hidden"
    ]
    {
      kernel = "linux";
      values = ["-fPIC"];
    }
    {
      kernel = "freebsd";
      values = ["-fPIC"];
    }
  ];

  patches = ./patches;
  builder = ./builder.sh;
  nativeBuildInputs = [
    which
  ];
}


let
  inherit (import ./../../util) nixpkgs requirePlatform patchDir fetchDepSrc strings;
  inherit (nixpkgs) stdenv;
  inherit (stdenv) mkDerivation;
in
  mkDerivation rec {
    pname = "boost";
    version = "1.70.0";
    # The underscored version used in filenames and elsewhere:
    version_ = strings.replace "_" "." version;

    src = fetchDepSrc {
      url = "https://dl.bintray.com/boostorg/release/${version}/source/${pname}_${version_}.tar.bz2";
      sha256 = "430ae8354789de4fd19ee52f3b1f739e1fba576f0aded0897c3c2bc00fb38778";
    };

    boostlibs = "chrono,filesystem,program_options,system,thread,test";
    configureOptions = [
      "variant=release" # FIXME: un-hardcode variant?
      "--layout=system"
      "threading=multi"
      "link=static"
      "-sNO_BZIP2=1"
      "-sNO_ZLIB=1"
    ] ++ (requirePlatform "[^-]*-[^-]*-linux-gnu" [
      "threadapi=pthread"
      "runtime-link=shared"
    ]);

    cxxflags = [
      "-std=c++11"
      "-fvisibility=hidden"
      (requirePlatform "[^-]*-[^-]*-linux-gnu" "-fPIC")
    ];

    patches = [
      # FIXME: Should this be applied on all build platforms?
      "${patchDir}/${pname}/darwin.diff"
    ];

    builder = ./builder.sh;
    nativeBuildInputs = [
      nixpkgs.which
    ];
  }


let
  inherit (import ./../../util) nixpkgs requirePlatform patchDir strings;
  inherit (nixpkgs) stdenv;
  inherit (stdenv) mkDerivation;
  sources = import ./../../sources;
in
  mkDerivation rec {
    pname = "boost";
    version = "1.70.0";
    # The underscored version used in filenames and elsewhere:
    version_ = strings.replace "_" "." version;
    src = "${sources}/${pname}_${version_}.tar.bz2";

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


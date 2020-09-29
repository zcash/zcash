with {
  inherit (import ./../../zcstd.nix)
    attrNames
    buildPlatform
    concatStrings
    fetchurl
    getAttr
    importTOML
    lib
    mkDerivation
    zcname
    zcsrc
    zcversion
    zcbuildutil
  ; 
};
mkDerivation rec {
  pname = "googletest";
  version = "1.8.0";
  src = fetchurl {
    url = "https://github.com/google/${pname}/archive/${pname}-${version}.tar.gz";
    sha256 = "58a6f4277ca2bc8565222b3bbd58a177609e9c488e8a72649359ba51450db7d8";
  };

  cxxflags = [
    "-std=c++11" 
  ] ++ (getAttr buildPlatform.parsed.kernel.name {
    "linux" = ["-fPIC"];
    "freebsd" = ["-fPIC"];
  });

  nativeBuildInputs = [
  ];

  inherit zcbuildutil;
  builder = ./builder.sh;
}



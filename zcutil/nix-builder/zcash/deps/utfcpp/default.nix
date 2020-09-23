with import ./../../../pkgs-pinned.nix;
with import ./../utils.nix;
stdenv.mkDerivation rec {
  pname = "utfcpp";
  version = "3.1";
  src = fetchurl {
    url = "https://github.com/nemtrif/$(package)/archive/${pname}-${version}.tar.gz";
    sha256 = "ab531c3fd5d275150430bfaca01d7d15e017a188183be932322f2f651506b096";
  };

  builder = ./builder.sh;
}

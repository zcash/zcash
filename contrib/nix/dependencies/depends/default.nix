## Dependencies managed by our depends system
final: previous: let
  mkDependsDerivation = import ./mk-depends-derivation.nix final;
in {
  db = mkDependsDerivation {
    pkg = final.db62;
    dependsPkgName = "bdb";
    url = pname: version: "https://download.oracle.com/berkeley-db/${pname}-${version}.tar.gz";
  };

  boost = mkDependsDerivation {
    pkg = final.boost180;
    url = pname: version: "https://boostorg.jfrog.io/artifactory/main/release/${builtins.replaceStrings ["_"] ["."] version}/source/${pname}_${version}.tar.bz2";
  };

  gtest = mkDependsDerivation {
    pkg = previous.gtest;
    dependsPkgName = "googletest";
    url = pname: version: "https://github.com/google/${pname}/archive/release-${version}.tar.gz";
  };

  # FIXME: This is not pinned precisely enough, but we need this level rather
  #        than the individual internal packages in order to set up the Nix
  #        stdenv.
  #
  # This subsumes libcxx, clang
  llvmPackages = final.llvmPackages_14;

  # libevent = mkDependsDerivation {
  #   pkg = previous.libevent;
  #   url = pname: version:
  #     "https://github.com/libevent/libevent/archive/release-${version}-stable.tar.gz";
  # };

  libsodium = mkDependsDerivation {
    pkg = previous.libsodium;
    url = pname: version: "https://download.libsodium.org/libsodium/releases/${pname}-${version}.tar.gz";
  };

  ccache = mkDependsDerivation {
    pkg = previous.ccache;
    dependsPkgName = "native_ccache";
    url = pname: version: "https://github.com/ccache/ccache/releases/download/v${version}/${pname}-${version}.tar.gz";
  };

  cctools = mkDependsDerivation {
    pkg = previous.cctools;
    dependsPkgName = "native_cctools";
    url = pname: version: "https://github.com/tpoechtrager/cctools-port/archive/${version}.tar.gz";
  };

  libtapi = mkDependsDerivation {
    pkg = previous.libtapi;
    dependsPkgName = "native_cctools";
    url = pname: version: "https://github.com/tpoechtrager/apple-libtapi/archive/${version}.tar.gz";
    infix = "_libtapi";
  };

  # cxx-rs = mkDependsDerivation {
  #   pkg = previous.cxx-rs;
  #   dependsPkgName = "native_cxxbridge";
  #   url = pname: version:
  #     "https://github.com/dtolnay/cxx/archive/refs/tags/${version}.tar.gz";
  # };

  # Depends only uses libtinfo, but outside of a Deb file, it’s only packaged as
  # part of ncurses. Here we approximate the depends version as best we can by
  # getting the oldest available version that isn’t older than what’s in
  # depends.
  ncurses =
    (previous.ncurses.override {
      abiVersion = "5";
    })
    .overrideAttrs (old: {
      version = "6.1";
      src = final.fetchurl {
        url = "https://invisible-island.net/archives/ncurses/ncurses-6.1.tar.gz";
        sha256 = "sha256-qgV+7rShTUcBAe/0WX1YM9zvWWUzG+NSjAjZnOuqDRc=";
      };
    });

  # need rust

  # utf8cpp = mkDependsDerivation {
  #   pkg = previous.utf8cpp;
  #   dependsPkgName = "utfcpp";
  #   url = pname: version:
  #     "https://github.com/nemtrif/utfcpp/archive/v${version}.tar.gz";
  # };

  zeromq = mkDependsDerivation {
    pkg = previous.zeromq;
    url = pname: version: "https://github.com/zeromq/libzmq/releases/download/v${version}/${pname}-${version}.tar.gz";
  };
}

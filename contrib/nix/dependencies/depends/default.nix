## Dependencies managed by our depends system
{nixpkgs-master}:
final: prev: let
  master = import nixpkgs-master {system = final.system;};

  mkDependsDerivation = import ./mk-depends-derivation.nix final;
in {
  db = mkDependsDerivation {
    pkg = final.db62;
    dependsPkgName = "bdb";
  };

  boost = mkDependsDerivation {
    pkg = master.boost181;
    # The URL substitution here is a bit too complex for `mkDependsDerivation`.
    url = pname: version: "https://boostorg.jfrog.io/artifactory/main/release/${builtins.replaceStrings ["_"] ["."] version}/source/${pname}_${version}.tar.bz2";
  };

  cmake = mkDependsDerivation {
    pkg = prev.cmake;
    dependsPkgName = "native_cmake";
  };

  gtest = mkDependsDerivation {
    pkg = prev.gtest;
    dependsPkgName = "googletest";
  };

  # FIXME: This is not pinned precisely enough, but we need this level rather
  #        than the individual internal packages in order to set up the Nix
  #        stdenv.
  #
  # This subsumes libcxx, clang
  llvmPackages = master.llvmPackages_15;

  # libevent = mkDependsDerivation {pkg = prev.libevent;};

  libsodium = mkDependsDerivation {pkg = prev.libsodium;};

  ccache = mkDependsDerivation {
    pkg = prev.ccache;
    dependsPkgName = "native_ccache";
  };

  cctools = mkDependsDerivation {
    pkg = prev.cctools;
    dependsPkgName = "native_cctools";
  };

  libtapi = mkDependsDerivation {
    pkg = prev.libtapi;
    dependsPkgName = "native_cctools";
    infix = "_libtapi";
  };

  # cxx-rs = mkDependsDerivation {
  #   pkg = prev.cxx-rs;
  #   dependsPkgName = "native_cxxbridge";
  # };

  # Depends only uses libtinfo, but outside of a Deb file, it’s only packaged as
  # part of ncurses. Here we approximate the depends version as best we can by
  # getting the oldest available version that isn’t older than what’s in
  # depends.
  ncurses =
    (prev.ncurses.override {
      abiVersion = "5";
    })
    .overrideAttrs (old: {
      version = "6.1";
      src = final.fetchurl {
        url = "https://invisible-island.net/archives/ncurses/ncurses-6.1.tar.gz";
        sha256 = "sha256-qgV+7rShTUcBAe/0WX1YM9zvWWUzG+NSjAjZnOuqDRc=";
      };
    });

  tl-expected = mkDependsDerivation {
    pkg = prev.tl-expected;
    dependsPkgName = "tl_expected";
  };

  # utf8cpp = mkDependsDerivation {
  #   pkg = prev.utf8cpp;
  #   dependsPkgName = "utfcpp";
  # };

  zeromq = mkDependsDerivation {pkg = prev.zeromq;};
}

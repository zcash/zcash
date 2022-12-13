## Dependencies managed by our depends system
final: previous: let
  mkDependsDerivation = import ./mk-depends-derivation.nix final;
in {
  db = mkDependsDerivation {
    pkg = final.db62;
    dependsPkgName = "bdb";
  };

  boost = mkDependsDerivation {
    pkg = final.boost180;
    # The URL substitution here is a bit too complex for `mkDependsDerivation`.
    url = pname: version: "https://boostorg.jfrog.io/artifactory/main/release/${builtins.replaceStrings ["_"] ["."] version}/source/${pname}_${version}.tar.bz2";
  };

  gtest = mkDependsDerivation {
    pkg = previous.gtest;
    dependsPkgName = "googletest";
  };

  # FIXME: This is not pinned precisely enough, but we need this level rather
  #        than the individual internal packages in order to set up the Nix
  #        stdenv.
  #
  # This subsumes libcxx, clang
  llvmPackages = final.llvmPackages_14;

  # libevent = mkDependsDerivation {pkg = previous.libevent;};

  libsodium = mkDependsDerivation {pkg = previous.libsodium;};

  ccache = mkDependsDerivation {
    pkg = previous.ccache;
    dependsPkgName = "native_ccache";
  };

  cctools = mkDependsDerivation {
    pkg = previous.cctools;
    dependsPkgName = "native_cctools";
  };

  libtapi = mkDependsDerivation {
    pkg = previous.libtapi;
    dependsPkgName = "native_cctools";
    infix = "_libtapi";
  };

  # cxx-rs = mkDependsDerivation {
  #   pkg = previous.cxx-rs;
  #   dependsPkgName = "native_cxxbridge";
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
  # };

  zeromq = mkDependsDerivation {pkg = previous.zeromq;};
}

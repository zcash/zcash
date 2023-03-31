## Dependencies managed by our depends system
{nixpkgs-master}:
final: prev: let
  master = import nixpkgs-master {system = final.system;};

  lib = import ./lib.nix final;
in {
  db = lib.mkDependsDerivation {
    pkg = final.db62;
    dependsPkgName = "bdb";
  };

  boost = lib.mkDependsDerivation {
    pkg = master.boost181;
    # The URL substitution here is a bit too complex for `lib.mkDependsDerivation`.
    url = pname: version: "https://boostorg.jfrog.io/artifactory/main/release/${builtins.replaceStrings ["_"] ["."] version}/source/${pname}_${version}.tar.bz2";
  };

  cmake = lib.mkDependsDerivation {
    pkg = prev.cmake;
    dependsPkgName = "native_cmake";
  };

  gtest = lib.mkDependsDerivation {
    pkg = prev.gtest;
    dependsPkgName = "googletest";
  };

  # FIXME: This is not pinned precisely enough, but we need this level rather
  #        than the individual internal packages in order to set up the Nix
  #        stdenv.
  #
  # This subsumes libcxx, clang
  llvmPackages = master.llvmPackages_15;

  # libevent = lib.mkDependsDerivation {pkg = prev.libevent;};

  libsodium = lib.mkDependsDerivation {pkg = prev.libsodium;};

  ccache = lib.mkDependsDerivation {
    pkg = prev.ccache;
    dependsPkgName = "native_ccache";
  };

  cctools = lib.mkDependsDerivation {
    pkg = prev.cctools;
    dependsPkgName = "native_cctools";
  };

  libtapi = lib.mkDependsDerivation {
    pkg = prev.libtapi;
    dependsPkgName = "native_cctools";
    infix = "_libtapi";
  };

  # cxx-rs = lib.mkDependsDerivation {
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

  rust-bin = prev.rust-bin.stable.${lib.readVersion "" (lib.fileContents "native_rust")}.default;

  tl-expected = lib.mkDependsDerivation {
    pkg = prev.tl-expected;
    dependsPkgName = "tl_expected";
  };

  # utf8cpp = lib.mkDependsDerivation {
  #   pkg = prev.utf8cpp;
  #   dependsPkgName = "utfcpp";
  # };

  zeromq = lib.mkDependsDerivation {pkg = prev.zeromq;};
}

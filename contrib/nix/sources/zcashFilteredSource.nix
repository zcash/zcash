let
  inherit (import ../util) selectSource;
in selectSource "zcash-filtered-source" "." [
  ".cargo"
  "Cargo.lock"
  "Cargo.toml"
  "Makefile.am"
  "autogen.sh"
  "build-aux"
  "configure.ac"
  "doc"
  "libzcashconsensus.pc.in"
  "qa/pull-tester"
  "share/genbuild.sh"
  "src"
  "zcutil/build-debian-package.sh"
  "zcutil/build.sh"
  "zcutil/fetch-params.sh"
]

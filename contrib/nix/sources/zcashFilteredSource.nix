let
  inherit (import ../util) selectSource;
in selectSource "zcash-filtered-source" "." [
  "autogen.sh"
  "build-aux"
  "configure.ac"
  "doc"
  "libzcashconsensus.pc.in"
  "Makefile.am"
  "qa/pull-tester"
  "src"
]

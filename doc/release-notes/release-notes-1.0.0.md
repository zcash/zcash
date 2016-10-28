Jack Grigg (8):
      Rework zcutil/build-debian-package.sh to place files correctly
      Add lintian check to zcutil/build-debian-package.sh
      Fix DEBIAN/control errors raised by lintian
      Build libsnark with -march=x86-64 instead of -march=native
      Disable the metrics screen on regtest
      Add the Zcash genesis blocks
      Update tests for new genesis blocks
      Update version strings to 1.0.0

Kevin Gallagher (6):
      Use fakeroot to build Debian package
      Update Debian package maintainer scripts
      Fixes executable mode of maintainer scripts
      Add DEBIAN/rules file (required by policy)
      Adds zcash.examples and zcash.manpages
      Run Lintian after built package is copied to $SRC_PATH


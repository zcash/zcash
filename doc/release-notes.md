(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Build system
------------

- The `--enable-lcov`, `--disable-tests`, and `--disable-mining` flags for
  `zcutil/build.sh` have been removed. You can pass these flags instead by using
  the `CONFIGURE_FLAGS` environment variable. For example, to enable coverage
  instrumentation (thus enabling "make cov" to work), call:

  ```
  CONFIGURE_FLAGS="--enable-lcov --disable-hardening" ./zcutil/build.sh
  ```

- The build system no longer defaults to verbose output. You can re-enable
  verbose output with `./zcutil/build.sh V=1`

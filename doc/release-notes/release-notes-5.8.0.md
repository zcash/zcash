Changelog
=========

Daira Emma Hopwood (2):
      Suppress compilation warnings for K&R-style prototypes when building bdb by adding `-Wno-deprecated-non-prototype` to `CFLAGS`.
      Suppress link warnings when building bdb by adding `-Wno-unused-command-line-argument` to `LDFLAGS`.

Jack Grigg (28):
      CI: Use latest stable Rust for book deployment
      CI: Rename build workflow to CI
      CI: Only run CI workflow once per PR
      CI: Switch to 8-core Ubuntu runners
      CI: Move matrix definition into a separate job
      CI: Rename `matrix.os` to `matrix.build_os`
      CI: Upload artifacts from regular build
      CI: Run btests and gtests
      CI: Split config flag bitrot builds into a separate job
      CI: Fix CCache path on macOS
      cargo vet prune
      cargo update
      depends: utfcpp 4.0.4
      depends: ZeroMQ 4.3.5
      depends: native_cmake 3.28.1
      depends: cxx 1.0.111
      qa: Bump postponed dependencies
      make-release.py: Versioning changes for 5.8.0-rc1.
      make-release.py: Updated manpages for 5.8.0-rc1.
      make-release.py: Updated release notes and changelog for 5.8.0-rc1.
      make-release.py: Updated book for 5.8.0-rc1.
      Update Rust dependencies for 5.8.0
      Postpone update to Rust 1.75.0
      depends: cxx 1.0.113
      depends: native_ccache 4.9
      depends: utfcpp 4.0.5
      make-release.py: Versioning changes for 5.8.0.
      make-release.py: Updated manpages for 5.8.0.

Yasser Isa (3):
      Delete contrib/ci-builders directory
      Delete contrib/ci-workers directory
      CI: Add sharding to GoogleTest job

dependabot[bot] (1):
      build(deps): bump actions/checkout from 3 to 4


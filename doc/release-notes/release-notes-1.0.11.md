Changelog
=========

Ariel Gabizon (3):
      make-release.py: Versioning changes for 1.0.11-rc1.
      make-release.py: Updated manpages for 1.0.11-rc1.
      make-release.py: Updated release notes and changelog for 1.0.11-rc1.

Daira Hopwood (7):
      Clean up imports to be pyflakes-checkable. fixes #2450
      For unused variables reported by pyflakes, either remove the variable,     suppress the warning, or fix a bug (if the wrong variable was used).     refs #2450
      Cosmetics (trailing whitespace, comment conventions, etc.)
      Alert 1004 (version 1.0.10 only)
      Remove UPnP support. fixes #2500
      Change wording in Security Warnings section of README.md.
      Document our criteria for adding CI workers. closes #2499

Jack Grigg (17):
      Pull in temporary release notes during the release process
      Ansible playbook for installing Zcash dependencies and Buildbot worker
      Variable overrides for Debian, Ubuntu and Fedora
      Variable overrides for FreeBSD
      Simplify Python installation, inform user if they need to manually configure
      Add test for issue #2444
      Add Buildbot worker setup to Ansible playbook
      Add steps for setting up a latent worker on Amazon EC2
      Add pyblake2 to required Python modules
      Remove Buildbot version from host file
      Add a separate Buildbot host info template for EC2
      Add pyflakes to required Python modules
      Add block download progress to metrics UI
      Correct and extend EstimateNetHeightInner tests
      Improve network height estimation
      make-release.py: Versioning changes for 1.0.11.
      make-release.py: Updated manpages for 1.0.11.

Simon Liu (3):
      Closes #2446 by adding generated field to listunspent.
      Fixes #2519. When sending from a zaddr, minconf cannot be zero.
      Fixes #2480. Null entry in map was dereferenced leading to a segfault.

Wladimir J. van der Laan (1):
      rpc: Add WWW-Authenticate header to 401 response

practicalswift (1):
      Net: Fix resource leak in ReadBinaryFile(...)


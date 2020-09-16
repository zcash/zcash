# Zcash auxillary tools

This directory has "auxillary" utilities for managing the depends system
and dependencies.

Specifically "auxillary" means *none of these should be required
for developer or release builds*, but instead may be used by other
infrastructure such as tarball link checking, caching, etc…

## utilities

### parse-dependency-urls.py

Parse out package source URL and hashes from the makefiles. It follows
convention and can't do sophisticated make interpretation.

### download-and-check.sh

Download and check sources based on output of `parse-dependency-urls.py`.

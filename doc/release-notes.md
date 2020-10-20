(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Migration to Clang and static libc++
------------------------------------

`zcashd` now builds its C++ (and C) dependencies entirely with a pinned version
of Clang, and statically links libc++ instead of dynamically linking libstdc++.
This migration enables us to reliably use newer C++ features while supporting
older LTS platforms, be more confident in the compiler's optimisations, and
leverage security features such as sanitisers and efficient fuzzing.

Additionally, because both Clang and rustc use LLVM as their backend, we can
optimise across the FFI boundary between them. This reduces the cost of moving
between C++ and Rust, making it easier to build more functionality in Rust
(though not making it costless, as we still need to work within the constraints
of the C ABI).

The system compiler is still used to compile a few native dependencies (used by
the build machine to then compile `zcashd` for the target machine). These will
likely also be migrated to use the pinned Clang in a future release.

# nix builds for zcash

Build zcashd using [nix](https://nixos.org).

## User Guide

This README is a user guide, meaning a guide for the humans who are building Zcash from source using `nix`. If you want to hack on the build system itself, please see the [Developer Guide](./README-dev.md).

### Who is this for?

This is for anyone who wants to build zcashd with `nix`, rather than the production `./zcutil/build.sh` system.

Generally, we advise you to use the production build approach unless you already know & love `nix`, you want to improve the build system based on `nix` (for example improving reproducibility or cross-compilation support), or you just like learning new things. We definitely love new collaborators!

### Getting Started

1. [install nix](https://nixos.org/guides/install-nix.html)
2. run `nix build ./nix`
3. Use/test the build results. `nix build` will create a symlink called `./result` that points to the build results inside the nix store. This is the only filesystem modification made outside of the nix store.
4. If the build fails, is missing a build artifact you want/need, or if you discover any other flaw during testing the build results, please [let us know](https://github.com/zcash/zcash/issues).

### Pros/Cons of using this approach
There are a few pros/cons to this approach versus the production approach.

Pros:
- Aside from `nix`, you don't need to install any other parts of the developer toolchain (for example `automake` or `bsdmainutils`). Because of this, it _may_ build successfully on more linux distros.
- No modification is made to the source tree because all build outputs go in the nix store.
- If you are building different branches of zcashd, even with different dependencies, caching of intermediate build artifacts (called "derivations" in nix) should do the right thing across a host-machine scope.
- Builds are potentially more likely to be reproducible across time, machines, and users, because this is a general feature goal of nix. It is, however, not guaranteed. (To be fair, the `./depends` system takes a few measures to maintain reproducibility.)
- If something goes wrong with the build process, there are a variety of post-hoc diagnostic tools nix provides. For example intermediate build outputs can be examined, the build environment can be recreated, logs of each stage are available, etc…
- Many dependency build processes live as standalone bash files (compared to `./depends` approach of sh packed into GNU make functions).

Cons:
- This approach is new and experimental. It doesn't yet support as wide a set of goals as the production system.
- This approach has not yet been audited or tested to the degree of the production approach.
- The nix build isn't the production build system, so it's not well tested by CI or other users.
- `nix` has a learning curve with a custom configuration language (`nix expressions`) and opinionated framework. The ./depends system is based on GNU Make and bourne shell.
- The current version builds zcashd itself within a nix build sandbox, so it doesn't cache intermediate results of the top-level `./configure; make` pipeline. This means it is slow to do a typical "edit app code, compile, run tests" loop.

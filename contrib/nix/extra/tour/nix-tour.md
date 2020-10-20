% zcashd Nix Tour
% Nathan Wilcox
% 2020-10-20

# Bootstrapping Demo

## Demo: Bootstrap

Find these slides in the build output.

# Project Overview

## Project Goals

- Exploratory Fun Friday Project.
- Scratching an itch: can we replace `./depends` w/ 3rd party tool?
- Fantasy: does the tool do cross compilation?
- Another Fantasy: can it replace gitian?

## ./depends

- We inherited `./depends` from Bitcoind.
- One (fateful) day I added `./zcutil/build.sh` because I'm allergic to typing >1 command.
- Features:
  - Manage build-time dependencies.
  - Pin to hash; build upstream source w/ local patches.
  - Including build toolchains as (meta-)dependencies!
  - Make builds reproducible.
  - Does exactly what we need in most cases, by dint of in-house solution.

## Alternative Tools Comparison

- I browsed a few projects for a half hour based on names I had recently heard.
- I looked for newish things that cleanly handle arbitrary dependencies and work across languages.
- I avoided build systems that either don't do dependency management really well.
- I have a strong bias againt build-code generation / transpilation stacks like `autoconf`, `cmake`, etc… Maybe autoconf has single-handedly ruined this approach for me, but I often find diagnosing a problem in the later stages becomes very messy/difficult.

## `bazel` and `meson`

- Two jumped out from my (very cursory) searching: `bazel` and `meson`.
- `meson` seemed a bit more focused on C++. Also, an old ticket about rust integration turned me off because a dev didn't seem to want to interact with the outside world.
- Both are more build systems-focused, ie: they replace `autoconf` / `make` / `gcc`.
- They do dependencies, but tend to assume every dependency fits into their world. (ex: `bazel` external dependencies…)

## `nix`

- Not a build system, per se, but a dependency/package management system.
- A "build orchestration" system: "fetch that source pinned to this hash, then build it w/ autoconf / cargo / setuptools.py …" similar to `homebrew`, `portage`, freebsd `pkg`, …
- Aims for:
  - Isolated independent runtimes of every package (so you can have concurrent installs of different versions or build flags for any package).
  - Reproducibility.
  - Cross compilation support.
- Anti-feature: "upgrade this shared library security update to ensure all apps are patched against a vuln". In nix, you'd rebuild all those apps.

See: `https://en.wikipedia.org/wiki/List_of_build_automation_software` which puts it in the "meta build" category.

## `nix` Pros

- Pin down a dependency graph.
- Flexible enough to handle any dependency (some require more effort to adapt).
- Reproducibility and cross-compilation are high priorities.
- Existing repository of community maintained packages (optional).
- IRC community is active and helpful and collaborative.
- Built in tool support for many common needs, ex: `nix-shell`.
- Purely functional config language w/ separate "evaluation" vs "instantiation" phases.
- Never modifies the source directory! Every source is stored immutably!

## `nix` Cons

- Linux + MacOS only.
- Assumes build results are to be installed in the nix store, creating "exportable packages" (ex: `.deb`) may be more work.
- Documentation is messy.
- Heavy learning curve; ex: bespoke config dynamically typed `nix` config language w/ some issues.
- Many tasks require retraining to "the nix way", ex: `nix-shell`.

## Demo: runtime linkage

Examine the runtime linkage of binaries to see `nix` implementation sausage:

    $ ldd ./zcash-v4.0.0-dev/bin/zcashd
    $ head ./zcash-v4.0.0-dev/bin/zcash-fetch-params

Now compare those to the `fetched-debian-binaries` derivation.

# The `nix.WIP` branch

## Exploration Goals

- Replace `./depends` (don't use it at all).
- Use _exact_ same sources as depends: urls, hashes, compile flags.
- Don't modify anything outside of `./contrib/nix` directory.
- Make it user friendly for build-users who don't know/care about nix.
- Make it easy-ish for auditors to know this is as safe as `./depends`.
- Learn `nix` and learn more about `./depends`.

## Exploration Status - the Good Parts

- It builds without `./depends` or `./zcutil/build.sh`. Woot!
- It doesn't modify anything outside of `./contrib/nix`. Yay!
- Package metadata is in clean TOML files. Nice!
- Package build scripts are shell script files. Whee!
- It uses `clang` to build all (supported) dependencies. Zing!
- It uses the literal `./depends` patch files directly. Zorp!
- I caught a few `./depends` bugs, filed some tickets/PRs. Zweep!

## Exploration Status - the Not-as-Good Parts

- The only verificaiton I've done is `zcashd -version`; no other testing.
- In particular, I'm not at all confident compilation flags are correctly replicated.
- It didn't end up directly implementing the `./depends` `clang` support - I used the nixpkgs version.
- It almost certainly has regressions for cross-compilation, no effort here yet.
- It includes way too much "custom nix code". (Still smaller than `./depends`, I think.)
- `nix` expects to be used as a package manager, so the build artifacts may not be easy to "export".

## Demo: Package Review

- Compare `./depends/packages` and `./contrib/nix/packages`
  - List files in both directories.
  - Look at the longest / shorted TOML and `.mk` files.
  - Grep for some hashes and patch files.
  - Run `sha256sum` on the sources derivation.
  - Compare a build shell script to a `./depends` makefile.
- Review the built code size report.

## Demo: Consistency Check

- Change a hash or delete a `./depends` package then do a nix build.

# Future Work

## Brainstormed roadmap

This is a roughly chronological brainstorm.

Tangent: nix is "above" the actual build system. Switching away from `autoconf` to a nicer build system for `zcashd` seems largely orthogonal to this nix design.

## Local test automation

Most of the `./qa/zcash/full_test_suite.py` tests assume a `./depends` build, for example by expecting build artifacts mixed into the source directory.

I would start to patch some of those to work with arbitrary target directories (ex: filtered source, final binaries, etc…).

Also, the tests themselves can be deployed as packages. This helps elucidate many messy internal dependencies and they can be made more modular over time.

## Auxillary CI Support

Add a CI builder that runs all the local and integration tests against a nix build, in addition to the "production path" CI. This would help detect changes in the main repo that break `nix` builds.

## Build End-user Packages

Can we build end-user packages (ex: debian binary tarballs, debian packages, docker images) with nix?

Once that works, can we add cross-compilation support for multiple target platforms?

I think both are possible, and feasible amount of work depending on package formats.

## Reproducible builds

Nix is already geared up for reproducible builds. It's right there in the path name. ;-)

We should verify that and make it easy for community collaborators to generate nix-based signed attestations.

## Switch over official releases

### The Dream

If nix is in CI, producing packages, cross-compiling, and reproducible, we can then retire both `./depends` and `gitian`.

The "developer" build tool chain would == the packaging/release tool chain, closing the gap on Continuous Deployment.

> ♫ I may be a dreamer, but I'm not the only one. ♩

## Alternatives

- We can keep refining `./depends` and gitian, slowly migrating everything to rust.
- Switch our package targets from debian packages to docker images. If that's the "user boundary" we could mess around with dependency management in a bunch of ways, for example, switch to debian packages for everything.
- Look into `bazel` or some of those tools more closely.

# Fin

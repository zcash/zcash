# Zcash nix-builder

This is an alternative build system for Zcash based on [nix](https://nixos.org/).

## Phase 0 TODOs

See the [Proposed Roadmap] below for phase descriptions.

1. Build a runnable zcashd/zcash-cli on the build platform with all non-compiler, non-crate `./depends` packages pinned to the same sources.
2. Pin rustc and cargo as per `./depends`.
3. Pin clang.
4. Undo the 'cp' of patch files and refer to them directly.
5. Remove unnecessary `builder`s where stdenv will do.
6. General nix cleanup and factoring out common code.
7. Add a docker wrapper.
8. Move to `./contrib`
9. Rebase to `master.`

## Proposed Roadmap

Note: This roadmap is merely a proposal by the contributors to this branch, and not necessarily adopted by the `zcashd` developers. Also, each subsequent phase becomes less certain. The end result might be no change to `zcashd` or only a subset of the phases in this proposed roadmap.

### Phase 0: Proof of Concept Prototype

During this phase, the nix-builder will live in a branch for those adventurous types that want to experiment with it.

### Phase 1: Merged to Master

#### Phase 1 Features

Phase 1 provides an alternative build system to `./zcutil/build.sh` and `./depends` for non-cross linux builds.

#### Phase 1 Zcash Integration

In this phase the nix-builder approach will be merged to master as a `contrib` feature, meaning it must not be on any critical path for building or releasing official `zcashd` releases/packages, nor will any developer be required to use it. Instead, a subset of dev users may use this to build their own dev versions.

During this phase, there's a continued risk that the official build system and the nix-builder will drift.

#### Phase 1 Requirements

**P1.R1.** It should be easy for any PR or code reviewer, even one unfamiliar with `./depends` or `nix`, to verify that:

- Both systems appear to define the same dependencies, including identical versions, URLs, and hashes.
- Both systems appear to define the same patches on dependencies, applied in the same order.
- The nix build file for top-level `zcash` in `./zcash/default.nix` appears to define many build tool prerequisites that are roughly similar to the required development tool dependencies defined on [Zcash readthedocs.io](https://zcash.readthedocs.io/en/latest/).
- The nix build appears to pin the set of `nixpkgs` packages to a specific hash.

**P1.R2.** It should be possible for a focused security-aware reviewer to verify that the "appears to X" statements above do indeed hold.

**P1.R3.** It should be possible for a dev not familiar with nix to build zcash with nix, either by installing nix on their dev system or by using a Docker-based wrapper.

**P1.R4.** The binaries built by the PR should pass all local automated tests.

### Phase 2: Continuous Integration Support

#### Phase 2 Features

There are no new features during this phase. Instead continuous integration support helps ensure changes to the production build system (`./zcutil/build.sh` + `./depends`) will require updating the nix build specs to match.

#### Phase 2 Zcash Integration

Even though the nix builder is not used in official developer flows or releases, CI support is added to ensure it is well maintained.

#### Phase 2 Requirements

**P2.R1.** All build and QA test automation runs against both the production `./depends` system and the nix build for `x86_64` on an officially supported linux distro.

### Phase 3 Cross-Platform Support

#### Phase 3 Features

Cross platform builds are introduces for all officially supported Zcash targets on at least one x86_64 linux build platform.

#### Phase 3 Zcash Integration

CI runs automated tests on all cross-compiled targets.

#### Phase 3 Requirements

**P3.R1.** CI automates the build of _each_ officially supported target platform via nix from a single _build_ host based on x86_64 on a favored linux distro.

**P3.R2.** CI automates running all automated single-host run-time tests for _each_ target platform using the binaries build by **P3.R1.**.
 That involves building for each officially supported _target_ platform from a single _build_ platform, then it involves running automated tests on each _target_ platform with those results.

### Phase 4: Deprecating ./depends

#### Phase 4 Features

This phase involves no new nix build features.

#### Phase 4 Zcash Integration

#### Phase 4 Requirements

**P4.R1.** All of CI switches promotes the nix build as primary, and makes the `./depends` secondary. (Because earlier phases already introduced CI coverage of nix builds along side the production `./depends` CI, this change should almost be a single toggle.)

**P4.R2.** Production releases switch from `./depends` to the nix build, including the gitian pipeline. Two achieve this safely a branch of `zcash-gitian` should be maintained that performs the `./depends`-based known-working build through at least two production `zcashd` releases.

**P4.R3.** At least two production `zcashd` releases must successfully complete using the nix builder within gitian with no reproducibility failures and no other blocking snags.

### Phase 5: Reproducible Nix Builds Belt-And-Suspenders

#### Phase 5 Features

The nix build constrains the build artifacts to be bytewise identical when built on any two systems from the same fresh clone of a specific `zcashd` git revision. The build streamlines signing a build artifact cryptographic hash attestation so that it is routine for many developers to create such attestations on each build.

#### Phase 5 Zcash Integration

Releases will still rely on gitian for artifact reproducibility checks, and the nix build attestations will provide a second layer initially.

Because the build system streamlines attestations, they can & should be gathered more frequently than once-per-release, and instead zcashd developers and even a wider set of volunteers should submit them for builds of `master` frequently (ie: once per PR merge for some devs, or once per week for volunteers).

#### Phase 5 Requirements

**P5.R1.** There is a published set of build system constraint requirements such that reproducible builds are only supported when those requirements are met. For example, if particular versions of nix or specific underlying build platforms (ie: nix-on-windows) are known to produce results different from other systems, we may exclude those arbitrarily-but-consistently to reduce the problem space.

**P5.R2.** Every build generates an unsigned build manifest for all artifacts. This includes intermediate nix derivations which are defined within the `zcashd` repository. If nix enables it, it should also encompass the entire transitive dependency graph of all derivations regardless of whether or not they are defined in the `zcashd` repo or in the upstream pinned `nixpkgs` repository.

**P5.R3.** The nix build makes it convenient to force a local build of the entire transitive derivation graph without relying on binary caching of servers. (FIXME: Research nix binary reproducibility more. Can derivations be locked down to fixed-output derivations?)

**P5.R4.** It's very easy and convenient for developers to opt-in to generating signed attestations of the build manifests. "very easy" means, for example, setting their GPG pubkey fingerprint in a config setting to produce this extra output. (FIXME: Research if there's a nix facility for this.)

**P5.R5.** Zcash CI/CD infrastructure automatically publishes infrastructure-signed attestations of every PR merge and every release.

##### Phase 5 Bonus Requirements

**P5.BR1.** There's a commandline tool any developer can use to publish their attestation *without requiring any zcash-developer gatekeeping of account management*. It can either post their attestation to a well-known viewing key, and/or push a commit to a their github fork of an attestation repo, then submit a PR.

**P5.BR2.** There are two tools *anyone can run* for aggregating attestations: one scans the well-known viewing key, and another searches github for all forks of a signature repo finding all attestation commits. Notice that anyone can do either **P5.BR1.** or **P5.BR2.** without relying on zcash devs and also zcash devs can't censor attestations (and github can't censor the blockchain-based attestations).

### Phase 6 Retire Gitian

#### Phase 6 Features

No new nix features past phase 5.

#### Phase 6 Zcash Integration

After the nix layer of attestations have proven themselves, we can drop gitian.

#### Phase 6 Requirements

**P6.R1.** There have been no attestation discrepencies between the nix layer and gitian builds for two zcashd production releases.

### Phase 7 End-user-package Nix Build Artifacts

#### Phase 7 Features

The top-level nix derivation for zcash is extended to produce end-user package artifacts. For example, debian and ubuntu `.deb` files, binary tarballs, Docker images, windows installers, mac installers.

#### Phase 7 Zcash Integration

As many end-user-packages as possible should be automated by nix generation, and where that's difficult, the existing package-specific approach will continue to be used.

#### Phase 7 Requirements

**P7.R1.** The "officially supported platforms" are extended to "officially supported packages", and each "package" format identifies a set of supported install/runtime platforms.

**P7.R2.** The top-level nix build expression generates all packages for all platforms.

**P7.R3.** There are more specific nix derivations for specific packages or builds, and it's easy for a dev to build just the pieces they need at the moment.

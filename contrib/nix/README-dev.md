# zcash nix build development

Want to hack on the nix build system for Zcash? Welcome! This is for you.

## Design

All of the nix-build specific files live inside `./nix`.

We call any `derivation` defined in this repo "locally defined" in contrast to derivations defined in the `nixpkgs` repo. All locally defined derivations use the same pinned snapshot of `nixpkgs` for non-local dependencies.

### Requirements

1. Users can build zcashd for their build platform (non-cross build) with `nix build ./nix` from the repo root.
2. All derivations are either locally defined in this repo, or they come from a specific pinned revision of `nixpkgs`.
3. The pinned hashes for dependencies built by the `./depends` system are parsed directly out of the `./depends` files, so that they only exist in one place and the two build systems cannot inadvertantly build different dependency revisions.

### Code Layout

#### Top Level ./nix

The top-level `./nix` directory should only contain things which pertain directly to users, aside from this single `README-dev.md`.

Any top-level parameters a user may care about should live in `config.toml`. Note, though, that this isn't for "custom user-specific configuration" since a goal of the nix build is reproducibility. Instead it's for high-level params that should be shared across all builds (thus it's in revision control).

In particular any `*.nix` file here should be a "high-level derivation" that users are likely to care about. The user guide points out that they may want to build any of those directly.

#### ./deps

This contains nix derivations for specific third party dependencies. Many of these correspond directly to `./depends` dependencies, though not all of them do.

#### ./util

This contains non-derivation nix utility code. Only nix build devs should care about this directory.

### Coding Conventions

#### nix conventions

We don't make use of the `callPackage` dependency injection convention. Every derivation directly imports its full dependencies. We currently don't need to provide a lot of flexibility about `zcashd` dependencies, since the `zcashd` built prioritizes building fewer well-known reproducible build configurations rather than supporting many build-user-specific customizations.

However, we do use one important dependency injection in the form of `./util/zcstdenv.nix` which pins the C/C++ tool chain to `clang`. This is done explicitly with calls to `zcstdenv.mkDerivation` and those derivations arrange for the build script's `stdenv` env variable to point to the `clang` toolchain. So from the point of view of builder scripts they still use `stdenv` transparently with this non-standard injected dependency.

##### Naming Styles

Use camelCase for naming files, nix identifiers, and shell variables. Rationale: this seems to be the most common standard in nix itself, and it also lets us maintain filename / identifier 1:1 correspondence.

Always, where possible, maintain 1:1 filename / identifier correspondence. For example, do `fooBar = import ./fooBar.nix` rather than `fooBar = import ./foo-bar.nix` or some other variation.

Prefer namespacing over prefixing, so for example instead of `zcLog = ...; zcCheck = ...;` use `zc = { log = ...; check = ...; }`.

##### Avoid `with`.

Avoid `with` because it's the only way to introduce bindings implicitly. It is similar to python `from thing import *` or rust `use thing::*`.

Rationale: This ensures that the source of any binding is textually visible within the same nix file that uses it. This is especially important since nix is dynamically typed and unstructured (for example an import can pull in a value of any type, depending on the source expression).

Instead of `with (srcexpr); (bodyexpr)` introduce explicit bindings with the `inherit` keyword to `let`. Example:

```
let
  inherit ( <srcexpr> ) foo bar;
  qux = <quxexpr> ;
in
  do_stuff [foo, bar, qux]
```

In this example `<blah>` is a metasyntactic variable standing in for any expression.

## Wishlist

These may not be chronological:

- Disable binary cache checks for zcash-specific derivations.
- Look into probabilistic binary cache skips (for reproducibility checks).
- Cross-compilation support.

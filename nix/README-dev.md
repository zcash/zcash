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

#### Naming Styles

Use camelCase for naming files, nix identifiers, and shell variables. Rationale: this seems to be the most common standard in nix itself, and it also lets us maintain filename / identifier 1:1 correspondence.

Always, where possible, maintain 1:1 filename / identifier correspondence. For example, do `fooBar = import ./fooBar.nix` rather than `fooBar = import ./foo-bar.nix` or some other variation.

Prefer namespacing over prefixing, so for example instead of `zcLog = ...; zcCheck = ...;` use `zc = { log = ...; check = ...; }`.

#### Avoid `with`.

Avoid `with` because it's the only way to introduce bindings implicitly. It is similar to python `from thing import *` or rust `use thing::*`.

Instead of `with (srcexpr); (bodyexpr)` introduce explicit bindings with the `inherit` keyword to `let`. Example:

```
let
  inherit (srcexpr) foo bar;
  qux = (quxexpr);
in
  do_stuff [foo, bar, qux]
```

In this manner, the source of every binding is textually visible in every file.

## TODO / Wishlist

These may not be chronological:

- A declarative approach to dependencies using `config.toml`.
- "Dev Cycle" support - make it convenient for a dev user to do this cycle:

  1. build a `config.site` derivation.
  2. Outside of nix, they run `./autogen.sh ; CONFIG_SITE='./result/config.site' ./configure`
  3. Now they can do an edit source/`make`/test test loop locally.

  Note this is currently well supported by the `./depends` system. TODO:
  Investigate using `nix-shell` for this.
- The stdenv generic builder is really complex. Should we use it or
  replace it? We currently use custom build scripts in 6 of 8 dependencies.
- Working non-cross build using all of `./depends` dependencies and toolchains.
- Cross-compilation support.

# zcash nix build development

Want to hack on the nix build system for Zcash? Welcome! This is for you.

## Design

All of the nix-build specific files live inside `./nix`.

We call any `derivation` defined in this repo "locally defined" in contrast to derivations defined in the `nixpkgs` repo. All locally defined derivations use the same pinned snapshot of `nixpkgs` for non-local dependencies.

### Requirements

1. Users can build zcashd for their build platform (non-cross build) with `nix build ./nix` from the repo root.
2. All derivations are either locally defined in this repo, or they come from a specific pinned revision of `nixpkgs`.
3. The pinned hashes for dependencies built by the `./depends` system are parsed directly out of the `./depends` files, so that they only exist in one place and the two build systems cannot inadvertantly build different dependency revisions.

### Coding Conventions

#### nix conventions

#### Naming Styles

Use `kebab-case.sh` for naming files (see [Stackoverflow for kebab case](https://stackoverflow.com/a/17820138)).

Use `camelCase` for nix identifiers.

Prefer namespacing over prefixing, so for example instead of `zcLog = ...; zcCheck = ...;` use `zc = { log = ...; check = ...; }`.

For names which are defined in this repo that are likely to have wide-spread scope, use a `zc` prefix, such as in a bash support function that's used in multiple build scripts. If at all possible, though, rely on deeper name-spaces, for example by grouping zcash-specific nix utilities into a set named `zc`.

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

- "Dev Cycle" support - make it convenient for a dev user to do this cycle:

  1. build a `config.site` derivation.
  2. Outside of nix, they run `./autogen.sh ; CONFIG_SITE='./result/config.site' ./configure`
  3. Now they can do an edit source/`make`/test test loop locally.

  Note this is currently well supported by the `./depends` system.
- Working non-cross build using all of `./depends` dependencies and toolchains.
- Cross-compilation support.

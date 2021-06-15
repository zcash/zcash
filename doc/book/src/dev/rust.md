# Rust in `zcashd`

`zcashd` is primarily a C++ codebase, but most new code is being written in Rust
where possible.

## Adding new dependencies in online-Rust mode

The `zcashd` build system pins all dependencies, and in order to faciliate
deterministic builds, `cargo` is configured to run in offline mode with vendored
crates. This means that if, for example, you add the `foobar` crate to
`Cargo.toml`, you will likely see an error similar to this:

```
$ cargo check
error: no matching package named `foobar` found
location searched: registry `https://github.com/rust-lang/crates.io-index`
required by package `librustzcash v0.2.0 (/path/to/zcash)`
```

Instead, you first need to build `zcashd` in online-Rust mode:
```
CONFIGURE_FLAGS=--enable-online-rust ./zcutil/build.sh
```

After doing so, you can add a new dependency as follows:

1. Add the new dependency to `Cargo.toml`.
2. Run `cargo check` to update the `Cargo.lock` file.
3. Commit `Cargo.toml` and `Cargo.lock`.

## Using a local Rust dependency

During development, you can use a locally checked out version of a dependency
by applying a [`cargo` patch](https://doc.rust-lang.org/cargo/reference/overriding-dependencies.html#the-patch-section).

For example, to use a local version of the `orchard` crate that includes a new
API, add the following patch to `Cargo.toml`:

```
[dependencies]
# This dependency is listed with a version, meaning it comes from crates.io; the
# patch goes into a [patch.crates-io] section.
orchard = "0.0"
...

[patch.crates-io]
# Comment out any existing patch, if present.
# orchard = { git = "https://github.com/zcash/orchard.git", rev = "..." }

# Add this patch (both relative and absolute paths work):
orchard = { path = "../relative/path/to/orchard" }
```

Usually you can apply a patch to use a locally checked out dependency without
needing to build `zcashd` in online-Rust mode. However, if your local changes
include a new dependency, you will need to ensure you are in online-Rust mode.

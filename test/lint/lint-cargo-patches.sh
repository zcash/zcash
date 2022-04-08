#!/usr/bin/env bash
#
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
#
# Check that any patched Cargo dependencies are correctly configured.

export LC_ALL=C

REPOBASE="$(dirname $0)/../.."
CARGO_TOML="$REPOBASE/Cargo.toml"
CONFIG_FILE="$REPOBASE/.cargo/config.offline"

CARGO_TOML_PATCH_PREFIX="[patch.crates-io]"
let CARGO_TOML_PATCH_START=$(grep -Fn "$CARGO_TOML_PATCH_PREFIX" $CARGO_TOML | cut -d: -f1)+1

REPLACE_LINE="replace-with = \"vendored-sources\""

EXIT_CODE=0

# Check that every patch specifies a revision.
# TODO: Implement.

# Check that every patch has a matching replacement.
for PATCH in $(tail -n+$CARGO_TOML_PATCH_START $CARGO_TOML | sed 's/.*git = "\([^"]*\)", rev = "\([^"]*\)".*/\1#\2/' | sort | uniq)
do
    PATCH_GIT=$(echo $PATCH | sed 's/#.*//')
    PATCH_REV=$(echo $PATCH | sed 's/.*#//')

    # Canonicalize the git URL (matching how Cargo treats them, so we don't over-lint).
    # https://github.com/rust-lang/cargo/blob/master/src/cargo/util/canonical_url.rs
    CANONICAL_GIT=$(echo $PATCH_GIT | sed 's/\/$//' | sed 's/.git$//')
    MAYBE_SUFFIX="\(.git\)\?/\?"

    CONFIG_START=$(grep -n "^\[source\.\"$CANONICAL_GIT$MAYBE_SUFFIX\"\]$" $CONFIG_FILE | cut -d: -f1)
    if [ -n "$CONFIG_START" ]; then
        HAVE_GIT=$(tail -n+$CONFIG_START $CONFIG_FILE | head -n4 | grep "^git = \"$CANONICAL_GIT$MAYBE_SUFFIX\"$")
        HAVE_REV=$(tail -n+$CONFIG_START $CONFIG_FILE | head -n4 | grep "^rev = \"$PATCH_REV\"$")
        HAVE_REPLACE=$(tail -n+$CONFIG_START $CONFIG_FILE | head -n4 | grep "^$REPLACE_LINE$")
    fi

    if [ -z "$CONFIG_START" ] || [ -z "$HAVE_GIT" ] || [ -z "$HAVE_REV" ] || [ -z "$HAVE_REPLACE" ]; then
        echo "Missing replacement in .cargo/config.offline:"
        echo "  [source.\"$PATCH_GIT\"]"
        echo "  git = \"$PATCH_GIT\""
        echo "  rev = \"$PATCH_REV\""
        echo "  $REPLACE_LINE"
        echo
        EXIT_CODE=1
    fi

    # Reset for the next iteration.
    HAVE_GIT=
    HAVE_REV=
    HAVE_REPLACE=
done

# Check for unused replacements.
# TODO: Implement.

exit ${EXIT_CODE}

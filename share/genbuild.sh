#!/bin/sh
#
# Generates a C/C++ header file (e.g., build.h) containing version information
# based on the current Git repository status (tag, hash, and dirty state).
#
# Copyright (c) 2016-2025 Project Developers
# Distributed under the MIT software license.

# Set locale to C for predictable string comparisons and sorting.
export LC_ALL=C

# --- Argument Parsing and Setup ---

# Check if the mandatory filename argument is provided.
if [ $# -lt 1 ]; then
    echo "Usage: $0 <output-filename> [srcroot-directory]"
    exit 1
fi

FILE="$1"
# Read the first line of the existing file to check if an update is needed later.
# Suppress errors if the file does not exist.
INFO=""
if [ -f "$FILE" ]; then
    INFO="$(head -n 1 "$FILE")"
fi

# Change directory if srcroot-directory is provided (positional argument 2).
if [ $# -gt 1 ]; then
    SRCROOT="$2"
    # Exit if the directory change fails
    cd "$SRCROOT" || exit 1
fi

# --- Git Version Retrieval ---

NEWINFO="// No build information available"
DESC=""

# Check if we are running in a Git work tree and if git is available.
if [ "${BITCOIN_GENBUILD_NO_GIT}" != "1" ] && command -v git >/dev/null 2>&1 && \
   [ "$(git rev-parse --is-inside-work-tree 2>/dev/null)" = "true" ]; then

    # Use 'git describe --always' which provides a short hash if no tag is found.
    # --dirty checks if there are uncommitted changes.
    # --tags includes lightweight tags.
    VERSION_STRING=$(git describe --always --dirty --tags 2>/dev/null)

    # Check for exact tag match. If the output is just a tag name (no hash suffix and no '-dirty'), 
    # use it as the clean build description.
    if ! echo "$VERSION_STRING" | grep -qE -- '(-[0-9]+-g|-dirty)'; then
        # This branch executes if we are exactly on a tag commit and the tree is clean.
        DESC="$VERSION_STRING"
        NEWINFO="#define BUILD_DESC \"$DESC\""
    else
        # Otherwise, use the full description string (e.g., v0.1.0-12-gHASH-dirty) 
        # as the suffix for debug builds.
        SUFFIX="$VERSION_STRING"
        NEWINFO="#define BUILD_SUFFIX \"$SUFFIX\""
    fi

fi

# --- Output Handling ---

# Only write to the output file if the content has changed 
# (to avoid unnecessary rebuilds triggered by file modification timestamps).
if [ "$INFO" != "$NEWINFO" ]; then
    echo "$NEWINFO" >"$FILE"
fi

#!/bin/sh
# Copyright (c) 2016-2024 The Zcash developers
# Copyright (c) 2012-2024 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

export LC_ALL=C

# 1. Argument Parsing & Validation
if [ $# -gt 1 ]; then
    SRC_DIR="$2"
    if [ ! -d "$SRC_DIR" ]; then
        echo "Error: Source directory '$SRC_DIR' does not exist."
        exit 1
    fi
    cd "$SRC_DIR" || exit 1
fi

if [ $# -gt 0 ]; then
    FILE="$1"
else
    echo "Usage: $0 <filename> <srcroot>"
    exit 1
fi

# 2. Read existing content (to prevent unnecessary file writes/rebuilds)
OLD_INFO=""
if [ -f "$FILE" ]; then
    OLD_INFO=$(head -n 1 "$FILE")
fi

DESC=""
SUFFIX=""
NEWINFO="// No build information available"

# 3. Git Information Extraction
# Check if git exists and if we are in a git repository
if [ "${BITCOIN_GENBUILD_NO_GIT}" != "1" ] && \
   command -v git >/dev/null 2>&1 && \
   [ "$(git rev-parse --is-inside-work-tree 2>/dev/null)" = "true" ]; then

    # Clean 'dirty' status of touched files that haven't been modified
    git diff >/dev/null 2>/dev/null

    # Get short commit hash
    COMMIT_ID=$(git rev-parse --short HEAD)

    # Check for dirty status
    if ! git diff-index --quiet HEAD --; then
        SUFFIX="-dirty"
    fi

    # Try to find an exact tag match for the current commit
    # This replaces the complex rev-list logic with a cleaner native git command
    if RAWDESC=$(git describe --exact-match --tags HEAD 2>/dev/null); then
        # If we are on a clean tag, use the tag as description
        if [ -z "$SUFFIX" ]; then
            DESC="$RAWDESC"
        else
            # If dirty, append dirty suffix to the commit hash, ignore tag
            SUFFIX="${COMMIT_ID}${SUFFIX}"
        fi
    else
        # Not on a tag, use commit hash + optional dirty suffix
        SUFFIX="${COMMIT_ID}${SUFFIX}"
    fi
fi

# 4. Construct C Macro
if [ -n "$DESC" ]; then
    NEWINFO="#define BUILD_DESC \"$DESC\""
elif [ -n "$SUFFIX" ]; then
    # CRITICAL FIX: Added quotes around $SUFFIX. 
    # Without quotes, a hash like 59887e8 creates a syntax error in C.
    NEWINFO="#define BUILD_SUFFIX \"$SUFFIX\""
fi

# 5. Write File (Only if changed)
if [ "$OLD_INFO" != "$NEWINFO" ]; then
    echo "$NEWINFO" > "$FILE"
    # echo "Build info updated: $NEWINFO"
fi

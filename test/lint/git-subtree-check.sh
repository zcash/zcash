#!/bin/sh
# Copyright (c) 2015 The Bitcoin Core developers
# Copyright (c) 2020-2022 The Zcash developers
# Distributed under the MIT software license.

# Exit immediately if a command exits with a non-zero status or fails in a pipeline.
set -eo pipefail

# Ensure consistent output and sorting behavior.
export LC_ALL=C

# --- Input Validation and Variable Setup ---

# Directory path of the subtree (strips trailing slash for consistency).
SUBTREE_DIR="${1%/}"
# Commit or reference to check against (defaults to HEAD).
COMMIT_REF="$2"

if [ -z "$COMMIT_REF" ]; then
    COMMIT_REF="HEAD"
fi

if [ -z "$SUBTREE_DIR" ]; then
    echo "Usage: $0 <subtree-dir> [commit-ref]" >&2
    exit 2
fi

# --- Helper Function to Find Subtree History ---

# Taken from git-subtree (Copyright (C) 2009 Avery Pennarun <apenwarr@gmail.com>)
find_latest_squash()
{
    local dir="$1"
    local sq=
    local main=
    local sub=
    
    # Search history for commits tagged with 'git-subtree-dir: <dir>'
    git log --grep="^git-subtree-dir: $dir/*\$" \
        --pretty=format:'START %H%n%s%n%n%b%nEND%n' "$COMMIT_REF" |
    while read a b _; do
        case "$a" in
            START) sq="$b" ;; # The commit hash where the merge/rejoin happened
            git-subtree-mainline:) main="$b" ;;
            git-subtree-split:) sub="$b" ;; # The upstream commit hash being pulled
            END)
                if [ -n "$sub" ]; then
                    # If we found a split commit, return the current commit (sq) 
                    # and the upstream commit (sub).
                    if [ -n "$main" ]; then
                        # Handle rejoin commits by treating the split commit as the source.
                        sq="$sub"
                    fi
                    echo "$sq" "$sub"
                    break
                fi
                sq=
                main=
                sub=
                ;;
        esac
    done
}

# --- Main Logic: Fetch and Check Hashes ---

# 1. Find the latest subtree update commit hashes.
LATEST_SQUASH_OUTPUT="$(find_latest_squash "$SUBTREE_DIR")"
if [ -z "$LATEST_SQUASH_OUTPUT" ]; then
    echo "ERROR: $SUBTREE_DIR is not recognized as a git subtree in $COMMIT_REF." >&2
    exit 2
fi

# Use read to safely capture the two resulting commit hashes.
read -r LAST_SQUASH_COMMIT UPSTREAM_SPLIT_COMMIT <<< "$LATEST_SQUASH_OUTPUT"

echo "Subtree directory: $SUBTREE_DIR"
echo "Last Squash Commit (local merge): $LAST_SQUASH_COMMIT"
echo "Upstream Split Commit (source): $UPSTREAM_SPLIT_COMMIT"
echo "---"

# 2. Get the actual tree hash of the subtree directory in the current commit.
TREE_ACTUAL=$(git ls-tree -d "$COMMIT_REF" "$SUBTREE_DIR" | head -n 1)
if [ -z "$TREE_ACTUAL" ]; then
    echo "FAIL: Subtree directory $SUBTREE_DIR not found in $COMMIT_REF." >&2
    exit 1
fi

# Extract type and tree hash from the output of git ls-tree.
read -r - tree_actual_type TREE_ACTUAL_HASH _ <<< "$TREE_ACTUAL"

echo "$SUBTREE_DIR in $COMMIT_REF currently refers to $tree_actual_type $TREE_ACTUAL_HASH"
if [ "$tree_actual_type" != "tree" ]; then
    echo "FAIL: Subtree directory $SUBTREE_DIR is not a tree object in $COMMIT_REF." >&2
    exit 1
fi

# 3. Check 1: Verify the current tree against the tree at the time of the last squash.
# The actual tree hash in the current commit should match the tree hash 
# when the last squash commit was created.
TREE_AT_SQUASH=$(git show -s --format="%T" "$LAST_SQUASH_COMMIT")
echo "$SUBTREE_DIR in $COMMIT_REF was last updated in commit $LAST_SQUASH_COMMIT (tree $TREE_AT_SQUASH)"

if [ "$TREE_ACTUAL_HASH" != "$TREE_AT_SQUASH" ]; then
    # Output the diff to stderr for easy viewing.
    git diff "$TREE_AT_SQUASH" "$TREE_ACTUAL_HASH" >&2
    echo "FAIL: Subtree directory was modified without a proper 'git subtree merge --squash'." >&2
    exit 1
fi

# 4. Check 2: Verify the current tree against the upstream commit tree.
# The actual tree hash must match the tree hash of the upstream commit that was merged.

# Check if the upstream commit object exists locally.
if [ "$(git cat-file -t "$UPSTREAM_SPLIT_COMMIT" 2>/dev/null)" != "commit" ]; then
    echo "WARNING: Upstream source commit $UPSTREAM_SPLIT_COMMIT is unavailable locally. Cannot verify upstream tree hash." >&2
    # Exit with success (0) or a warning (e.g., 3) depending on CI requirements.
    exit 0
fi

TREE_SUBTREE_HASH=$(git show -s --format="%T" "$UPSTREAM_SPLIT_COMMIT")
echo "$SUBTREE_DIR in $COMMIT_REF matches the upstream commit $UPSTREAM_SPLIT_COMMIT (tree $TREE_SUBTREE_HASH)"

if [ "$TREE_ACTUAL_HASH" != "$TREE_SUBTREE_HASH" ]; then
    echo "FAIL: Local tree hash ($TREE_ACTUAL_HASH) does not match the upstream source tree hash ($TREE_SUBTREE_HASH)." >&2
    echo "This indicates an issue during the last merge or a mismatch in the upstream history." >&2
    exit 1
fi

echo "GOOD: Subtree integrity verified."

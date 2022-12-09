#!/usr/bin/env bash
set -euo pipefail
shopt -s inherit_errexit
# shellcheck disable=SC2154 # `s` is assigned, but shellcheck can’t tell.
trap 's=$?; echo "$0: Error on line "$LINENO": $BASH_COMMAND"; exit $s' ERR

### This finds all the git subtrees that exist at HEAD and prints them,
###
### > path/to/subtree – nix hash for that path
### > most recent commit subject
### > first few lines of commit body
###
### If the most recent commit was a merge, it prints out the information for the
### _second_ parent (the one merged into the current branch).

function print_commit_info() {
    commit_hash=$1
    git log --max-count 1 --format="%s%n%b" "${commit_hash}" | head --lines=5
}

mapfile -t subtrees < \
        <(git log | grep git-subtree-dir | sed -e 's/^.*: //g' | sort | uniq)

for subtree in "${subtrees[@]}"; do
    if [[ -d "$subtree" ]]
    then
        echo "$subtree – $(nix hash path $subtree)"
        IFS=" " read -r -a parents \
           <<< "$(git log --max-count 1 --format="%P" $subtree)"
        if [[ -v parents && ${#parents[@]} -gt 1 ]]; then
            print_commit_info "${parents[1]}"
        else
            print_commit_info "${subtree}"
        fi
        echo
    fi
done

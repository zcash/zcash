#!/bin/bash

# This script basically performs what bazel run //tools:build_cleaner would do
# but does so in a way that the actual script is run while the Bazel lock is
# not held. This is needed because the script runs bazel query which results
# in a deadlock if it would be run directly with bazel run.

function die() {
  echo "$1"
  exit 1
}

function cleanup() {
  rm "$runcmd"
}

runcmd="$(mktemp /tmp/build_cleaner.XXXXXX)" || die "Could not create tmp file"
trap "cleanup" EXIT

bazel run --script_path="$runcmd" //tools:build_cleaner || exit $?
[ -x "$runcmd" ] || die "File $runcmd not executable"

WORKING_DIRECTORY="$(pwd)" BAZEL_CMD=bazel "$runcmd"

#!/usr/bin/env bash
source $(dirname ${BASH_SOURCE[0]})/../contrib/strict-mode.bash
export LC_ALL=C
set -x

# Warning: This deletes tags on "origin", so point that at the right target!
#
# Note: It doesn't delete any local tags.

ZCASH_TAG_RGX='^v[0-9]+.[0-9]+.[0-9]+.z[0-9]+'
MAXJOBS=7

i=0

for nonzctag in $(git ls-remote origin \
                         | grep refs/tags/ \
                         | grep -v '\^{}$' \
                         | sed 's,^.*refs/tags/,,' \
                         | grep -Ev "$ZCASH_TAG_RGX"
                 )
do
    git push origin ":refs/tags/${nonzctag}" &
    i=$((i + 1))
    [ "$i" -ge "$MAXJOBS" ] && wait -n
done

wait

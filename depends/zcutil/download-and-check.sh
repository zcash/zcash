#!/bin/bash
#
# Attempt to download all the files and check their hashes against the
# expected hashes.

set -efuo pipefail

ZCUTIL="$(dirname "$(readlink -f "$0")")"
OUTDIR="$(mktemp --directory --tmpdir 'zc-dep-sources.XXX')"

echo "Downloading to: $OUTDIR"
cd "$OUTDIR"

for entry in $("$ZCUTIL/parse-dependency-urls.py" | jq -c '.[]')
do
    url="$(echo "$entry" | jq -r .url)"  
    fname="$(echo "$url" | sed 's,^.*/\([^/]*\)$,\1,')"
    sha256="$(echo "$entry" | jq -r .sha256)"  

    echo "= Downloading: $url -> $fname"
    echo "${sha256}  ${fname}" >> sha256sums.txt
    wget "$url" || echo "Warning: failed to download $url"
    echo
done

echo "Checking sha256sums.txt:"
sha256sum --check --quiet ./sha256sums.txt

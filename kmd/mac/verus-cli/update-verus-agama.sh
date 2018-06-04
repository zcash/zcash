#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "Usage: ./update-verus-agama.sh path/to/agama/directory."
    echo "No arguments supplied, no updates applied."
    exit 1
fi

PASSED=$1

if [ -d "${PASSED}" ] ; then
    if [ -f "${PASSED}/komodod" ] ; then
        cp komodod ${PASSED}
        cp komodo-cli ${PASSED}
        exit 0
    else
        echo "No komodod found in ${PASSED}"
        echo "Copying komodod and komodo-cli anyway"
        cp komodod ${PASSED}
        cp komodo-cli ${PASSED}
        exit 0
    fi
else
    echo "Pass the Agama-darwin-x64 directory on the command line."
    echo "${PASSED} is not a valid directory.";
    exit 1
fi


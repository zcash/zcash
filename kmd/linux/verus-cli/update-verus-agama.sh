#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "Usage: ./update-verus-agama.sh path/to/agama/directory."
    echo "No arguments supplied, no updates applied."
    exit 1
fi

PASSED=$1

if [ -d "${PASSED}" ] ; then
    if [ -f "${PASSED}/resources/app/assets/bin/linux64/komodod" ] ; then
        cp komodod ${PASSED}/resources/app/assets/bin/linux64/
        cp komodo-cli ${PASSED}/resources/app/assets/bin/linux64/
        exit 0
    else
        echo "No komodod found in ${PASSED}/resources/app/assets/bin/linux64/"
        echo "Copying komodod and komodo-cli anyway"
        cp komodod ${PASSED}/resources/app/assets/bin/linux64/
        cp komodo-cli ${PASSED}/resources/app/assets/bin/linux64/
        exit 0
    fi
else
    echo "Pass the Agama-linux-x64 directory on the command line."
    echo "${PASSED} is not a valid directory.";
    exit 1
fi


#!/bin/bash
if [  -z ${assetchain+x} ]; then

    /komodo/src/komodo-cli $1 $2 $3 $4

else

    /komodo/src/komodo-cli -ac_name=$assetchain $1 $2 $3 $4

fi


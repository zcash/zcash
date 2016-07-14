#!/bin/bash

. ./zcutil/base-zc-ui.vars


if [ -e "$KEYFILES_DIR" ]
then
    for keyfile in $(ls "$KEYFILES_DIR" | grep pubkey)
    do
        echo "$(echo "$keyfile" | sed -e "s/.*\([0-9]\+\).*/\1/"): $(cat "$KEYFILES_DIR/$keyfile")"
    done
else
    echo 'No addresses.'
fi

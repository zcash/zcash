#!/bin/bash

. ./zcutil/base-zc-ui.vars


if [[ $# != 2 ]]
then
    echo 'Usage: ./zcutils/zc-get-balance <ADDRESS-NUMBER> <ENCRYPTED-BUCKET>'
    exit 0
fi

PRIVKEYFILE="$KEYFILES_DIR/zcash-address-$1.privkey"
if ! [ -f "$PRIVKEYFILE" ]
then
    echo "Invalid Zcash address number: $1"
    exit 1
fi

ZCSECRETKEY=$(cat $PRIVKEYFILE)

RESULT=$("$ZCASH_CLI" zcrawreceive "$ZCSECRETKEY" "$2")
COINVAL=$(parse_result "$RESULT" 'amount')
BUCKET=$(parse_result "$RESULT" 'bucket')
EXISTS=$(parse_result "$RESULT" 'exists')

if [ "$BUCKET" == "$BAD_DECRYPT" ]
then
    echo "The encrypted bucket was not sent to address $1."
    exit 0
fi

echo "Received $COINVAL ZEC."
if [ "$EXISTS" == 'false' ]
then
    echo "The ZEC currently does not exist."
fi

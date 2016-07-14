#!/bin/bash

. ./zcutil/base-zc-ui.vars


if [[ $# < 1 ]] || [ "$1" == '-?' ] || [ "$1" == '--help' ]
then
    echo 'Usage: ./zcutils/zc-get-balance <ADDRESS-NUMBER>'
    exit 0
fi

PRIVKEYFILE="$KEYFILES_DIR/zcash-address-$1.privkey"
if ! [ -f "$PRIVKEYFILE" ]
then
    echo "Invalid Zcash address number: $1"
    exit 1
fi

ZCSECRETKEY=$(cat $PRIVKEYFILE)

TOT=0
SPENDABLE=0
if [ -e "$BUCKETS_DIR" ]
then
    for bucketfile in $(ls "$BUCKETS_DIR" | grep "zcash-encbucket-$1")
    do
        RESULT=$("$ZCASH_CLI" zcrawreceive "$ZCSECRETKEY" "$(cat "$BUCKETS_DIR/$bucketfile")")
        COINVAL=$(parse_result "$RESULT" 'amount')
        EXISTS=$(parse_result "$RESULT" 'exists')
        TOT=$(python -c "print $TOT+$COINVAL")
        if [ "$EXISTS" == 'true' ]
        then
            SPENDABLE=$(python -c "print $SPENDABLE+$COINVAL")
        fi
    done
fi

echo "Total: $TOT ZEC"
echo "Spendable: $SPENDABLE ZEC"

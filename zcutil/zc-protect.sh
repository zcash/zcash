#!/bin/bash

. ./zcutil/base-zc-ui.vars


if [[ $# < 1 ]] || [ "$1" == '-?' ] || [ "$1" == '--help' ]
then
    echo 'Usage: ./zcutils/zc-protect <ADDRESS-NUMBER> [TXID]'
    echo
    echo 'Without TXID, will protect the first unprotected output'
    exit 0
fi

PUBKEYFILE="$KEYFILES_DIR/zcash-address-$1.pubkey"
if ! [ -f "$PUBKEYFILE" ]
then
    echo "Invalid Zcash address number: $1"
    exit 1
fi

ZCADDRESS=$(cat $PUBKEYFILE)

RESULT=$("$ZCASH_CLI" listunspent)
if [ -n "$2" ]
then
    while [ "$TXID" != "$2" ] && [ -n "$RESULT" ]
    do
        TXID=$(parse_result "$RESULT" 'txid')
        VOUT=$(parse_result "$RESULT" 'vout')
        AMT=$(parse_result "$RESULT" 'amount')
        if [ -n "$AMT" ]
        then
            RESULT=$(cut_result "$RESULT" 'amount')
        else
            RESULT=''
        fi
    done
    TOT=$AMT
else
    # TODO Grab all outputs instead of first
    TXID=$(parse_result "$RESULT" 'txid')
    VOUT=$(parse_result "$RESULT" 'vout')
    AMT=$(parse_result "$RESULT" 'amount')
    TOT=$AMT
fi

echo "Total coins to protect: $TOT"
echo -n 'Enter fee (default: 0.1): '
read FEE
if ! [ -n "$FEE" ]
then
    FEE=0.1
fi

if [ "$(python -c "$FEE < 0")" ]
then
    echo 'Fee cannot be negative.'
    exit 0
elif [ "$(python -c "$FEE >= $TOT")" ]
then
    echo 'Fee larger than protected coin value.'
    exit 0
fi

NET=$(python -c "print $TOT-$FEE")

echo 'Protecting transparent coins...'
RAWTX=$("$ZCASH_CLI" createrawtransaction "[{\"txid\":\"$TXID\",\"vout\":$VOUT}]" '{}')

RESULT=$("$ZCASH_CLI" zcrawpour "$RAWTX" '{}' "{\"$ZCADDRESS\":$NET}" "$TOT" "$FEE")
BUCKET=$(parse_result "$RESULT" 'encryptedbucket1')
RAWTXN=$(parse_result "$RESULT" 'rawtxn')

RESULT=$("$ZCASH_CLI" signrawtransaction "$RAWTXN")
SIGNEDRAWTXN=$(parse_result "$RESULT" 'hex')

echo 'Sending transaction...'
RESULT=$("$ZCASH_CLI" sendrawtransaction "$SIGNEDRAWTXN")
POUR_TXID=$(parse_result "$RESULT" 'hex')
echo "$BUCKET" > "$BUCKETS_DIR/zcash-encbucket-$1.$(echo $BUCKET | sha1sum | sed -e 's/\s.*//')"

echo "Coins protected in transaction $POUR_TXID."
echo 'You must wait for this transaction to enter a block before you can use your Zcash.'

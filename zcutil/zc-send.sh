#!/bin/bash

. ./zcutil/base-zc-ui.vars


if [ $# != 3 ]
then
    echo 'Usage: ./zcutils/zc-send <ADDRESS-NUMBER> <TO-ADDRESS> <AMOUNT>'
    exit 0
fi

PUBKEYFILE="$KEYFILES_DIR/zcash-address-$1.pubkey"
PRIVKEYFILE="$KEYFILES_DIR/zcash-address-$1.privkey"
if ! [ -f "$PUBKEYFILE" ] || ! [ -f "$PRIVKEYFILE" ]
then
    echo "Invalid Zcash address number: $1"
    exit 1
fi

ZCADDRESS=$(cat $PUBKEYFILE)
ZCSECRETKEY=$(cat $PRIVKEYFILE)

TO_ADDRESS=$2
AMOUNT=$3

echo 'Finding suitable coin...'
COINVAL=0
for bucketfile in $(ls "$BUCKETS_DIR" | grep "zcash-encbucket-$1")
do
    RESULT=$("$ZCASH_CLI" zcrawreceive "$ZCSECRETKEY" "$(cat "$BUCKETS_DIR/$bucketfile")")
    COINVAL=$(parse_result "$RESULT" 'amount')
    BUCKET=$(parse_result "$RESULT" 'bucket')
    EXISTS=$(parse_result "$RESULT" 'exists')
    if [ "$EXISTS" == 'true' ] && [ "$(python -c "print 1 if $COINVAL > $AMOUNT else 0")" == 1 ]
    then
        BUCKETFILE="$BUCKETS_DIR/$bucketfile"
        break
    fi
done

if [ "$EXISTS" == 'false' ] || [ "$(python -c "print 1 if $COINVAL < $AMOUNT else 0")" == 1 ]
then
    echo 'No suitable coin found'
    exit
fi

echo "Coin found with value: $COINVAL"
echo -n 'Enter fee (default: 0.1): '
read FEE
if ! [ -n "$FEE" ]
then
    FEE=0.1
fi

EXCESS=$(python -c "print $COINVAL-$AMOUNT")

if [ "$(python -c "$FEE < 0")" ]
then
    echo 'Fee cannot be negative.'
    exit 0
elif [ "$(python -c "print 1 if $FEE > $EXCESS else 0")" == 1 ]
then
    echo 'Fee larger than remaining coin value.'
    exit 0
fi

CHANGE=$(python -c "print $EXCESS-$FEE")

echo 'Creating transaction...'
RAWTX=$("$ZCASH_CLI" createrawtransaction '[]' '{}')

RESULT=$("$ZCASH_CLI" zcrawpour "$RAWTX" "{\"$BUCKET\":\"$ZCSECRETKEY\"}" "{\"$TO_ADDRESS\":$AMOUNT,\"$ZCADDRESS\":$CHANGE}" 0 "$FEE")
TO_BUCKET=$(parse_result "$RESULT" 'encryptedbucket1')
CHANGE_BUCKET=$(parse_result "$RESULT" 'encryptedbucket2')
RAWTXN=$(parse_result "$RESULT" 'rawtxn')

RESULT=$("$ZCASH_CLI" signrawtransaction "$RAWTXN")
SIGNEDRAWTXN=$(parse_result "$RESULT" 'hex')

echo 'Sending transaction...'
RESULT=$("$ZCASH_CLI" sendrawtransaction "$SIGNEDRAWTXN")
POUR_TXID=$(parse_result "$RESULT" 'hex')
echo "$CHANGE_BUCKET" > "$BUCKETS_DIR/zcash-encbucket-$1.$(echo $CHANGE_BUCKET | sha1sum | sed -e 's/\s.*//')"
mv $BUCKETFILE $SPENT_BUCKETS_DIR

echo "Transaction complete: $POUR_TXID"
echo 'Send the following encrypted bucket to the recipient:'
echo
echo "$TO_BUCKET"

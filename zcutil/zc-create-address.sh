#!/bin/bash

. ./zcutil/base-zc-ui.vars


NEXT_NUM="$KEYFILES_DIR/next_address_num"
NUM=$(cat "$NEXT_NUM" || echo 1)

echo 'Making new Zcash address...'
RESULT=$("$ZCASH_CLI" zcrawkeygen)

ZCADDRESS=$(parse_result "$RESULT" 'zcaddress')
ZCSECRETKEY=$(parse_result "$RESULT" 'zcsecretkey')

echo "$ZCADDRESS" >"$KEYFILES_DIR/zcash-address-$NUM.pubkey"
echo "$ZCSECRETKEY" >"$KEYFILES_DIR/zcash-address-$NUM.privkey"

echo "Address generated. In other commands, use address number $NUM to use this address."

let NUM=NUM+1
echo $NUM > "$NEXT_NUM"

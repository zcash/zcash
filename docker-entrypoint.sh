#!/bin/bash
set -e

if [ "$1" = 'zcash-cli' -o "$1" = 'zcashd' ]; then
	mkdir -p ${ZCASH_DATA}
	chown -R zcash:zcash ${ZCASH_DATA}

	if [ ! -s ${ZCASH_CONF} ]; then
		cat <<-EOF > ${ZCASH_CONF}
		rpcpassword=${ZCASH_RPC_PASSWORD:-`head -c 32 /dev/urandom | base64`}
		rpcuser=${ZCASH_RPC_USER:-zcash}
		mainnet=1
		addnode=mainnet.z.cash
		EOF
	fi

fi

exec gosu zcash "$@"

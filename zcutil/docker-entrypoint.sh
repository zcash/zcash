#!/bin/bash

#set -ex

echo "...Checking komodo.conf"

if [ ! -e "$HOME/.komodo/komodo.conf" ]; then
    mkdir -p $HOME/.komodo

    echo "...Creating komodo.conf"
    cat <<EOF > $HOME/.komodo/komodo.conf
rpcuser=${rpcuser:-komodorpc}
rpcpassword=${rpcpassword:-`dd if=/dev/urandom bs=33 count=1 2>/dev/null | base64`}
txindex=1
bind=${listenip:-127.0.0.1}
rpcbind=${listenip:-127.0.0.1}
addnode=5.9.102.210
addnode=78.47.196.146
addnode=178.63.69.164
addnode=88.198.65.74
addnode=5.9.122.241
addnode=144.76.94.38
EOF

    cat $HOME/.komodo/komodo.conf
fi

echo "...Checking fetch-params"
$HOME/zcutil/fetch-params.sh

echo "Initialization completed successfully"
echo
if [ $# -gt 0 ]; then

    args=("$@")

elif [  -z ${assetchain+x} ]; then

    args=("-gen -genproclimit=${genproclimit:-2} -pubkey=${pubkey}")

else

    args=("-pubkey=${pubkey} -ac_name=${assetchain} -addnode=${seednode}")

fi

echo
echo "****************************************************"
echo "Running: komodod ${args[@]}"
echo "****************************************************"

exec komodod ${args[@]}

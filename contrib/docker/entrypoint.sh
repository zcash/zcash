#!/usr/bin/env bash

export LC_ALL=C
set -eo pipefail

if [[ ${1} == "--version" ]];then
  zcashd --version
  exit 0
fi

env | sort | grep ZCASHD || true
export ZCASHD_CMD='zcashd -printtoconsole'

if [[ ! -n ${ZCASHD_NETWORK} ]];then
  export ZCASHD_NETWORK=mainnet
fi

case ${ZCASHD_NETWORK} in
  testnet)
    ZCASHD_CMD+=" -testnet -addnode=testnet.z.cash "
    ;;
  mainnet)
    ZCASHD_CMD+=" -addnode=mainnet.z.cash "
    ;;
  *)
    echo "Error, unknown network: ${ZCASHD_NETWORK}"
    exit 1
    ;;
esac

if [[ -n "${ZCASHD_SHOWMETRICS}" ]];then ZCASHD_CMD+=" -showmetrics=${ZCASHD_SHOWMETRICS}";fi
if [[ -n "${ZCASHD_LOGIPS}" ]];then ZCASHD_CMD+=" -logips=${ZCASHD_LOGIPS}";fi
if [[ -n "${ZCASHD_EXPERIMENTALFEATURES}" ]];then ZCASHD_CMD+=" -experimentalfeatures=${ZCASHD_EXPERIMENTALFEATURES}";fi
if [[ -n "${ZCASHD_GEN}" ]];then ZCASHD_CMD+=" -gen=${ZCASHD_GEN}";fi
if [[ -n "${ZCASHD_ZSHIELDCOINBASE}" ]];then ZCASHD_CMD+=" -zshieldcoinbase=${ZCASHD_ZSHIELDCOINBASE}";fi
if [[ -n "${ZCASHD_RPCUSER}" ]];then ZCASHD_CMD+=" -rpcuser=${ZCASHD_RPCUSER}";fi
if [[ -n "${ZCASHD_RPCPASSWORD}" ]];then ZCASHD_CMD+=" -rpcpassword=${ZCASHD_RPCPASSWORD}";fi
if [[ -n "${ZCASHD_RPCBIND}" ]];then ZCASHD_CMD+=" -rpcbind=${ZCASHD_RPCBIND}";fi
if [[ -n "${ZCASHD_RPCPORT}" ]];then ZCASHD_CMD+=" -rpcport=${ZCASHD_RPCPORT}";fi
if [[ -n "${ZCASHD_ALLOWIP}" ]];then ZCASHD_CMD+=" -rpcallowip=${ZCASHD_ALLOWIP}";fi
if [[ -n "${ZCASHD_TXINDEX}" ]];then ZCASHD_CMD+=" -txindex";fi
if [[ -n "${ZCASHD_INSIGHTEXPLORER}" ]];then ZCASHD_CMD+=" -insightexplorer";fi
if [[ -n "${ZCASHD_ZMQPORT}" && -n "${ZCASHD_ZMQBIND}" ]];then
  ZCASHD_CMD+=" -zmqpubhashblock=tcp://${ZCASHD_ZMQBIND}:${ZCASHD_ZMQPORT}"
  ZCASHD_CMD+=" -zmqpubhashtx=tcp://${ZCASHD_ZMQBIND}:${ZCASHD_ZMQPORT}"
  ZCASHD_CMD+=" -zmqpubrawblock=tcp://${ZCASHD_ZMQBIND}:${ZCASHD_ZMQPORT}"
  ZCASHD_CMD+=" -zmqpubrawtx=tcp://${ZCASHD_ZMQBIND}:${ZCASHD_ZMQPORT}"
  ZCASHD_CMD+=" -zmqpubhashblock=tcp://${ZCASHD_ZMQBIND}:${ZCASHD_ZMQPORT}"
fi

zcash-fetch-params
touch .zcash/zcash.conf
echo "Starting: ${ZCASHD_CMD}"
eval exec "${ZCASHD_CMD}" "${@}"

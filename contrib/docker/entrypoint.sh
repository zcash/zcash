#!/usr/bin/env bash
source $(dirname ${BASH_SOURCE[0]})/../strict-mode.bash
export LC_ALL=C

if [[ -v 1 && ${1} == "--version" ]];then
  zcashd --version
  exit 0
fi

env | sort | grep ZCASHD || true
export ZCASHD_CMD='zcashd -printtoconsole'

if [[ ! -v ZCASHD_NETWORK || -z ${ZCASHD_NETWORK} ]];then
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

if [[ -v ZCASHD_SHOWMETRICS && -n "${ZCASHD_SHOWMETRICS}" ]];then
   ZCASHD_CMD+=" -showmetrics=${ZCASHD_SHOWMETRICS}";
fi
if [[ -v ZCASHD_LOGIPS && -n "${ZCASHD_LOGIPS}" ]];then
   ZCASHD_CMD+=" -logips=${ZCASHD_LOGIPS}";
fi
if [[ -v ZCASHD_EXPERIMENTALFEATURES && -n "${ZCASHD_EXPERIMENTALFEATURES}" ]];then
   ZCASHD_CMD+=" -experimentalfeatures=${ZCASHD_EXPERIMENTALFEATURES}";
fi
if [[ -v ZCASHD_GEN && -n "${ZCASHD_GEN}" ]];then
   ZCASHD_CMD+=" -gen=${ZCASHD_GEN}";
fi
if [[ -v ZCASHD_EQUIHASHSOLVER && -n "${ZCASHD_EQUIHASHSOLVER}" ]];then
   ZCASHD_CMD+=" -equihashsolver=${ZCASHD_EQUIHASHSOLVER}";
fi
if [[ -v ZCASHD_GENPROCLIMIT && -n "${ZCASHD_GENPROCLIMIT}" ]];then
   ZCASHD_CMD+=" -genproclimit=${ZCASHD_GENPROCLIMIT}";
fi
if [[ -v ZCASHD_ZSHIELDCOINBASE && -n "${ZCASHD_ZSHIELDCOINBASE}" ]];then
   ZCASHD_CMD+=" -zshieldcoinbase=${ZCASHD_ZSHIELDCOINBASE}";
fi
if [[ -v ZCASHD_RPCUSER && -n "${ZCASHD_RPCUSER}" ]];then
   ZCASHD_CMD+=" -rpcuser=${ZCASHD_RPCUSER}";
fi
if [[ -v ZCASHD_RPCPASSWORD && -n "${ZCASHD_RPCPASSWORD}" ]];then
   ZCASHD_CMD+=" -rpcpassword=${ZCASHD_RPCPASSWORD}";
fi
if [[ -v ZCASHD_RPCBIND && -n "${ZCASHD_RPCBIND}" ]];then
   ZCASHD_CMD+=" -rpcbind=${ZCASHD_RPCBIND}";
fi
if [[ -v ZCASHD_RPCPORT && -n "${ZCASHD_RPCPORT}" ]];then
   ZCASHD_CMD+=" -rpcport=${ZCASHD_RPCPORT}";
fi
if [[ -v ZCASHD_ALLOWIP && -n "${ZCASHD_ALLOWIP}" ]];then
   ZCASHD_CMD+=" -rpcallowip=${ZCASHD_ALLOWIP}";
fi
if [[ -v ZCASHD_TXINDEX && -n "${ZCASHD_TXINDEX}" ]];then
   ZCASHD_CMD+=" -txindex";
fi
if [[ -v ZCASHD_INSIGHTEXPLORER && -n "${ZCASHD_INSIGHTEXPLORER}" ]];then
   ZCASHD_CMD+=" -insightexplorer";
fi
if [[ -v ZCASHD_PROMETHEUSPORT && -n "${ZCASHD_PROMETHEUSPORT}" ]];then
   ZCASHD_CMD+=" -prometheusport=${ZCASHD_PROMETHEUSPORT}";
fi
if [[ -v ZCASHD_METRICSIP && -n "${ZCASHD_METRICSIP}" ]];then
   ZCASHD_CMD+=" -metricsallowip=${ZCASHD_METRICSIP}";
fi
if [[ -v ZCASHD_ZMQPORT && -v ZCASHD_ZMQBIND && -n "${ZCASHD_ZMQPORT}" && -n "${ZCASHD_ZMQBIND}" ]];then
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

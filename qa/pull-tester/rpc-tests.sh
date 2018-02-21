#!/bin/bash
set -e -o pipefail

CURDIR=$(cd $(dirname "$0"); pwd)
# Get BUILDDIR and REAL_BITCOIND
. "${CURDIR}/tests-config.sh"

export BITCOINCLI=${BUILDDIR}/qa/pull-tester/run-bitcoin-cli
export BITCOIND=${REAL_BITCOIND}

#Run the tests

testScripts=(
    'paymentdisclosure.py'            #Good
    'prioritisetransaction.py'        #Good
    'wallet_treestate.py'             #Good
    'wallet_protectcoinbase.py'       #Good
    'wallet_shieldcoinbase.py'        #Good
    'wallet.py'                       #Good
    'wallet_nullifiers.py'            #Good
    'wallet_1941.py'                  #Good
    'listtransactions.py'             #Good
    'mempool_resurrect_test.py'       #Good
    'txn_doublespend.py'              #Good
    'txn_doublespend.py --mineblock'  #Good
    'getchaintips.py'                 #Good
    'rawtransactions.py'              #Good
    'rest.py'                         #Good
    'mempool_spendcoinbase.py'        #Good
    'mempool_coinbase_spends.py'      #Good
    'mempool_tx_input_limit.py'       #Good
    'httpbasics.py'                   #Good
    'zapwallettxes.py'                #Good
    'proxy_test.py'                   #Good
    'merkle_blocks.py'                #Good
    'fundrawtransaction.py'           #Good
    'signrawtransactions.py'          #Good
    'walletbackup.py'                 #Good
    'key_import_export.py'            #Good
    'nodehandling.py'                 #Good
    'reindex.py'                      #Good
    'decodescript.py'                 #Good
    'disablewallet.py'                #Good
    'zcjoinsplit.py'                  #Good
    'zcjoinsplitdoublespend.py'       #Good
    'zkey_import_export.py'           #Good
    'getblocktemplate.py'             #Good
    'bip65-cltv-p2p.py'               #Good
    'bipdersig-p2p.py'                #Good
);
testScriptsExt=(
    'getblocktemplate_longpoll.py'    #Good
    # 'getblocktemplate_proposals.py' #TODO - FIXME
    # 'pruning.py'                    #TODO - FIXME
    'forknotify.py'                   #Good
    # 'hardforkdetection.py'          #TODO - FIXME
    # 'invalidateblock.py'            #TODO - FIXME
    'keypool.py'                      #Good
    # 'receivedby.py'                 #TODO - FIXME
    'rpcbind_test.py'                 #Good
#   'script_test.py'                  
    # 'smartfees.py'                  #TODO - FIXME
    'maxblocksinflight.py'            #Good
    # 'invalidblockrequest.py'        #TODO - FIXME
    # 'p2p-acceptblock.py'            #TODO - FIXME
);

if [ "x$ENABLE_ZMQ" = "x1" ]; then
  testScripts+=('zmq_test.py')
fi

if [ "x$ENABLE_PROTON" = "x1" ]; then
  testScripts+=('proton_test.py')
fi

extArg="-extended"
passOn=${@#$extArg}

successCount=0
declare -a failures

function runTestScript
{
    local testName="$1"
    shift

    echo -e "=== Running testscript ${testName} ==="

    if eval "$@"
    then
        successCount=$(expr $successCount + 1)
        echo "--- Success: ${testName} ---"
    else
        failures[${#failures[@]}]="$testName"
        echo "!!! FAIL: ${testName} !!!"
    fi

    echo
}

if [ "x${ENABLE_BITCOIND}${ENABLE_UTILS}${ENABLE_WALLET}" = "x111" ]; then
    for (( i = 0; i < ${#testScripts[@]}; i++ ))
    do
        if [ -z "$1" ] || [ "${1:0:1}" == "-" ] || [ "$1" == "${testScripts[$i]}" ] || [ "$1.py" == "${testScripts[$i]}" ]
        then
            runTestScript \
                "${testScripts[$i]}" \
                "${BUILDDIR}/qa/rpc-tests/${testScripts[$i]}" \
                --srcdir "${BUILDDIR}/src" ${passOn}
        fi
    done   
    for (( i = 0; i < ${#testScriptsExt[@]}; i++ ))
    do
        if [ "$1" == $extArg ] || [ "$1" == "${testScriptsExt[$i]}" ] || [ "$1.py" == "${testScriptsExt[$i]}" ]
        then
            runTestScript \
                "${testScriptsExt[$i]}" \
                "${BUILDDIR}/qa/rpc-tests/${testScriptsExt[$i]}" \
                --srcdir "${BUILDDIR}/src" ${passOn}
        fi
    done

    echo -e "\n\nTests completed: $(expr $successCount + ${#failures[@]})"
    echo "successes $successCount; failures: ${#failures[@]}"

    if [ ${#failures[@]} -gt 0 ]
    then
        echo -e "\nFailing tests: ${failures[*]}"
        exit 1
    else
        exit 0
    fi
else
  echo "No rpc tests to run. Wallet, utils, and bitcoind must all be enabled"
fi

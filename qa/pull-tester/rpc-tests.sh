#!/bin/bash
set -e -o pipefail

CURDIR=$(cd $(dirname "$0"); pwd)
# Get BUILDDIR and REAL_BITCOIND
. "${CURDIR}/tests-config.sh"

export BITCOINCLI=${BUILDDIR}/qa/pull-tester/run-bitcoin-cli
export BITCOIND=${REAL_BITCOIND}

#Run the tests

testScripts=(
    'dpow.py'
    'dpowconfs.py'
    'ac_private.py'
    'verushash.py'
    'paymentdisclosure.py'
    'prioritisetransaction.py'
    'wallet_treestate.py'
    'wallet_anchorfork.py'
    'wallet_changeindicator.py'
    'wallet_import_export.py'
    'wallet_protectcoinbase.py'
    'wallet_shieldcoinbase_sprout.py'
    'wallet_shieldcoinbase_sapling.py'
    'wallet_listreceived.py'
    'wallet_mergetoaddress.py'
    'wallet.py'
    'wallet_overwintertx.py'
    'wallet_persistence.py'
    'wallet_nullifiers.py'
    'wallet_1941.py'
    'wallet_addresses.py'
    'wallet_sapling.py'
    'wallet_listnotes.py'
    'listtransactions.py'
    'mempool_resurrect_test.py'
    'txn_doublespend.py'
    'txn_doublespend.py --mineblock'
    'getchaintips.py'
    'rawtransactions.py'
    'rest.py'
    'mempool_spendcoinbase.py'
    'mempool_reorg.py'
    'mempool_tx_input_limit.py'
    'mempool_nu_activation.py'
    'mempool_tx_expiry.py'
    'httpbasics.py'
    'zapwallettxes.py'
    'proxy_test.py'
    'merkle_blocks.py'
    'fundrawtransaction.py'
    'signrawtransactions.py'
    'signrawtransaction_offline.py'
    'walletbackup.py'
    'key_import_export.py'
    'nodehandling.py'
    'reindex.py'
    'addressindex.py'
    'timestampindex.py'
    'spentindex.py'
    'decodescript.py'
    'blockchain.py'
    'disablewallet.py'
    'zcjoinsplit.py'
    'zcjoinsplitdoublespend.py'
    'ivk_import_export.py'
    'zkey_import_export.py'
    'reorg_limit.py'
    'getblocktemplate.py'
    'bip65-cltv-p2p.py'
    'bipdersig-p2p.py'
    'p2p_nu_peer_management.py'
    'rewind_index.py'
    'p2p_txexpiry_dos.py'
    'p2p_node_bloom.py'
    'regtest_signrawtransaction.py'
    'finalsaplingroot.py'
);
testScriptsExt=(
    'getblocktemplate_longpoll.py'
    'getblocktemplate_proposals.py'
    'pruning.py'
    'forknotify.py'
    'hardforkdetection.py'
    'invalidateblock.py'
    'keypool.py'
    'receivedby.py'
    'rpcbind_test.py'
#   'script_test.py'
    'smartfees.py'
    'maxblocksinflight.py'
    'invalidblockrequest.py'
#    'forknotify.py'
    'p2p-acceptblock.py'
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

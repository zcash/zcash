#!/usr/bin/bash

# This script makes the neccesary transactions to migrate
# coin between 2 assetchains on the same -ac_cc id

set -e

source=TXSCL
target=TXSCL000
address="RFw7byY4xZpZCrtkMk3nFuuG1NTs9rSGgQ"
amount=1

# Alias for running cli on source chain
cli_source="komodo-cli -ac_name=$source"

# Raw tx that we will work with
txraw=`$cli_source createrawtransaction "[]" "{\"$address\":$amount}"`

# Convert to an export tx
exportData=`$cli_source migrate_converttoexport $txraw $target $amount`
exportRaw=`echo $exportData | jq -r .exportTx`
exportPayouts=`echo $exportData | jq -r .payouts`

# Fund
exportFundedData=`$cli_source fundrawtransaction $exportRaw`
exportFundedTx=`echo $exportFundedData | jq -r .hex`

# Sign
exportSignedData=`$cli_source signrawtransaction $exportFundedTx`
exportSignedTx=`echo $exportSignedData | jq -r .hex`

# Send
echo "Sending export tx"
$cli_source sendrawtransaction $exportSignedTx

read -p "Wait for a notarisation to KMD, and then two more notarisations from the target chain, and then press enter to continue"

# Create import
importTx=`$cli_source migrate_createimporttransaction $exportSignedTx $payouts`
importTx=`komodo-cli migrate_completeimporttransaction $importTx`

# Send import
komodo-cli -ac_name=$target sendrawtransaction $importTx

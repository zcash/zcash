# MigrateCoin protocol



## ExportCoins tx:



```

vin:

  [ any ]

vout:

  - amount: {burnAmount}

    script: OP_RETURN "send to ledger {id} {voutsHash}"

```



* ExportCoin is a standard tx which burns coins in an OP_RETURN



## ImportCoins tx:



```

vin:

  - txid:   0000000000000000000000000000000000000000000000000000000000000000

    idx:    0

    script: CC_EVAL(EVAL_IMPORTCOINS, {momoProof},{exportCoin}) OP_CHECKCRYPTOCONDITION_UNILATERAL

vout:

  - [ vouts matching voutsHash in exportCoin ]

```



* ImportCoin transaction has no signature

* ImportCoin is non malleable

* ImportCoin satisfies tx.IsCoinBase()

* ImportCoin uses a new opcode which allows a one sided check (no scriptPubKey)

* ImportCoin must contain CC opcode EVAL_IMPORTCOINS

* ImportCoin fees are equal to the difference between burnAmount in exportCoins and the sum of outputs.

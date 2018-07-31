# Payment Disclosure (Experimental Feature)

**Summary**

Use RPC calls `z_getpaymentdisclosure` and `z_validatepaymentdisclosure` to reveal details of a shielded payment.

**Who should read this document**

Frequent users of shielded transactions, payment processors, exchanges, block explorer

### Experimental Feature

This is an experimental feature.  Enable it by launching `zcashd` with flags:

    zcashd -experimentalfeatures -paymentdisclosure -debug=paymentdisclosure -txindex=1

These flags can also be set as options in `zcash.conf`.

All nodes that generate or validate payment disclosures must run with `txindex=1` enabled.

### Background

Payment Disclosure is an implementation of the work-in-progress Payment Disclosure ZIP [1].

The ZIP describes a method of proving that a payment was sent to a shielded address. In the typical case, this means enabling a sender to present a proof that they transferred funds to a recipient's shielded address. 

[1] https://github.com/zcash/zips/pull/119

### Example Use Case

Alice the customer sends 10 ZEC to Bob the merchant at the shielded address shown on their website.  However, Bob is not sure if he received the funds.

Alice's node is running with payment disclosure enabled, so Alice generates a payment disclosure and provides it to Bob, who verifies the payment was made.

If Bob is a bad merchant, Alice can present the payment disclosure to a third party to validate that payment was indeed made.

### Solution

A payment disclosure can be generated for any output of a JoinSplit using the RPC call:

    z_getpaymentdisclosure txid js_index output_index (message)

An optional message can be supplied.  This could be used for a refund address or some other reference, as currently it is not common practice to (ahead of time) include a refund address in the memo field when making a payment.

To validate a payment disclosure, the following RPC call can be used:

    z_validatepaymentdisclosure hexdata

### Example

Generate a payment disclosure for the first joinsplit, second output (index starts from zero):

    zcash-cli z_getpaymentdisclosure 79189528d611e811a1c7bb0358dd31343033d14b4c1e998d7c4799c40f8b652b 0 1 "Hello"
    
This returns a payment disclosure in the form of a hex string:

    706462ff000a3722aafa8190cdf9710bfad6da2af6d3a74262c1fc96ad47df814b0cd5641c2b658b0fc499477c8d991e4c4bd133303431dd5803bbc7a111e811d6289518790000000000000000017e861adb829d8cb1cbcf6330b8c2e25fb0d08041a67a857815a136f0227f8a5342bce5b3c0d894e2983000eb594702d3c1580817d0374e15078528e56bb6f80c0548656c6c6f59a7085395c9e706d82afe3157c54ad4ae5bf144fcc774a8d9c921c58471402019c156ec5641e2173c4fb6467df5f28530dc4636fa71f4d0e48fc5c560fac500

To validate the payment disclosure:

    zcash-cli z_validatepaymentdisclosure HEXDATA
    
This returns data related to the payment and the payment disclosure:

    {
      "txid": "79189528d611e811a1c7bb0358dd31343033d14b4c1e998d7c4799c40f8b652b",
      "jsIndex": 0,
      "outputIndex": 1,
      "version": 0,
      "onetimePrivKey": "1c64d50c4b81df47ad96fcc16242a7d3f62adad6fa0b71f9cd9081faaa22370a",
      "message": "Hello",
      "joinSplitPubKey": "d1c465d16166b602992479acfac18e87dc18065f6cefde6a002e70bc371b9faf",
      "signatureVerified": true,
      "paymentAddress": "ztaZJXy8iX8nrk2ytXKDBoTWqPkhQcj6E2ifARnD3wfkFwsxXs5SoX7NGmrjkzSiSKn8VtLHTJae48vX5NakvmDhtGNY5eb",
      "memo": "f600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
      "value": 12.49900000,
      "commitmentMatch": true,
      "valid": true
    }

The `signatureVerified` field confirms that the payment disclosure was generated and signed with the joinSplitPrivKey, which should only be known by the node generating and sending the transaction 7918...652b in question.

### Where is the data stored?

For all nodes, payment disclosure does not touch `wallet.dat` in any way.

For nodes that only validate payment disclosures, no data is stored locally.

For nodes that generate payment disclosures, a LevelDB database is created in the node's datadir.  For most users, this would be in the folder:

    $HOME/.zcash/paymentdisclosure
    
If you decide you don't want to use payment disclosure, it is safe to shut down your node and delete the database folder.

### Security Properties

Please consult the work-in-progress ZIP for details about the protocol, security properties and caveats.

### Reminder

Feedback is most welcome!

This is an experimental feature so there are no guarantees that the protocol, database format, RPC interface etc. will remain the same in the future.

### Notes

Currently there is no user friendly way to help senders identify which joinsplit output index maps to a given payment they made.  It is possible to construct this from `debug.log`.  Ideas and feedback are most welcome on how to improve the user experience.

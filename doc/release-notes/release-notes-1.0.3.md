Jack Grigg (5):
      Add --disable-tests flag to zcutils/build.sh
      Correctly set CNoteData::witnessHeight when decrementing witness caches
      Copy over CNoteData::witnessHeight when updating wallet tx
      Add code comments about CNoteData::witnessHeight
      Clear witnessHeight and nWitnessCacheSize in ClearNoteWitnessCache

Jay Graber (4):
      Document z_sendmany error code messages in payment-api.md
      s/Bitcoin/Zcash in JSONRPCError
      Change format of z_sendmany error code documentation.
      Release-notes.py script to generate release notes and add contributors to authors.md

Sean Bowe (7):
      Regression test for constraint system violation.
      Improve accuracy of constraint system violation diagnostics.
      Add tests for witness `element` and tree `last` methods. Strengthen testing by inserting a different commitment into the tree at each step.
      Initialize after profiling messages are suppressed.
      Process verification keys to perform online verification.
      Add test that `last` and `element` will throw exception when the tree is blank.
      Anchors and nullifiers should always be inherited from the parent cache.

Simon (8):
      Closes #1833.  Format currency amounts in z_sendmany error message.
      Closes #1680, temporary fix for rpc deadlock inherited from upstream.
      Set default minrelaytxfee to 1000 zatoshis to match upstream.
      Mempool will accept tx with joinsplits and the default z_sendmany fee.
      Track the correct change witness across chained joinsplits
      Closes #1854. z_sendmany selects more utxos to avoid dust change output.
      Partial revert of bd87e8c: file release-notes-1.0.2.md to 343b0d6.
      Fix threading issue when initializing public params.

ayleph (1):
      Correct spelling error in z_sendmany error output


Jack Grigg:
      Equihash: Only compare the first n/(k+1) bits when sorting.
      Randomise the nonce in the block header.
      Clear mempool before using it for benchmark test, fix parameter name.
      Fix memory leak in large tx benchmark.

Sean Bowe:
      Increase block size to 2MB and update performance test.
      Make sigop limit `20000` just as in Bitcoin, ignoring our change to the blocksize limit.
      Remove the mainnet checkpoints.
      Fix performance test for block verification.
      Make `validatelargetx` test more accurate.

Taylor Hornby:
      Add example mock test of CheckTransaction.

aniemerg:
      Suppress Libsnark Debugging Info.


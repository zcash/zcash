Jack Grigg (4):
      Equihash: Only compare the first n/(k+1) bits when sorting.
      Randomise the nonce in the block header.
      Clear mempool before using it for benchmark test, fix parameter name.
      Fix memory leak in large tx benchmark.

Sean Bowe (5):
      Increase block size to 2MB and update performance test.
      Make sigop limit `20000` just as in Bitcoin, ignoring our change to the blocksize limit.
      Remove the mainnet checkpoints.
      Fix performance test for block verification.
      Make `validatelargetx` test more accurate.

Taylor Hornby (1):
      Add example mock test of CheckTransaction.

aniemerg (1):
      Suppress Libsnark Debugging Info.

# Regtest

_Regtest_ ("regression test") is one of the three _network types_
supported by Zcash, the others being `testnet` and `mainnet`.
Regtest is an entirely local, self-contained mode -- your node or nodes
do not talk with peers out in the world. It gives you complete
control of the state of the blockchain and the sequence of events.
You start with an empty blockchain (just the genesis block, block 0).
Blocks can be mined instantly, and must be created explicitly. 

You can run one or more regtest nodes on the same system.
The [RPC tests](https://github.com/zcash/zcash/tree/master/qa/rpc-tests)
use `regtest` mode (usually starting multiple nodes), but you may use it
manually and interactively to learn, test, and experiment. Most often
just one node is used in this case.

## Example session

Here's a sample session (after you've built `zcashd` and `zcash-cli`):

```
$ mkdir /tmp/regtest-datadir
$ cat <<EOF >/tmp/regtest-datadir/zcash.conf
regtest=1
rpcuser=u
rpcpassword=p
rpcport=18132
EOF
$ src/zcashd -daemon -datadir=/tmp/regtest-datadir
```

Watch `tmp/regtest-datadir/regtest/debug.log` to see what `zcashd` is doing.
It may also be useful to start `zcashd` with `-debug` to generate much
more logging. Now we can send RPCs to the node:

```
$ src/zcash-cli -datadir=/tmp/regtest-datadir getblockchaininfo
{
  "chain": "regtest",
  "blocks": 0,
  "initial_block_download_complete": false,
  "headers": 0,
  (...)
}
# Generate (mine) three blocks:
$ src/zcash-cli -datadir=/tmp/regtest-datadir generate 3
[
  "05040271f43f78e3870a88697eba201aa361ea5802c69eadaf920ff376787242",
  "0469f2df43dda69d521c482679b2db3c637b1721222511302584ac75e057c859",
  "0ab7a26e7b3b5dfca077728de90da0cfd1c49e1edbc130a52de4062b1aecac75"
]
$ src/zcash-cli -datadir=/tmp/regtest-datadir getblockchaininfo
{
  "chain": "regtest",
  "blocks": 3,
  "initial_block_download_complete": true,
  "headers": 3,
  (...)
}
$ src/zcash-cli -datadir=/tmp/regtest-datadir stop
Zcash server stopping
$ 
```
## Network upgrades

Zcash has adopted a series of
[network upgrades](https://github.com/zcash/zcash/blob/master/src/consensus/upgrades.cpp).
On `mainnet` and `testnet`, these activate at
fixed, known block heights ([example](https://github.com/zcash/zcash/blob/master/src/chainparams.cpp#L117)).
In `regtest` mode, you determine the activation heights. Upgrades may occur at
any height greater than 0, and multiple upgrades can occur at the same height. The upgrades
have a strict ordering (as shown in the upgrades source file); for example, Canopy can't
be activated before Blossom. 

You specify the upgrade heights using multiple `-nuparams=`_\<branch-id\>_ arguments.
(The branch IDs are available in the
[upgrades.cpp file](https://github.com/zcash/zcash/blob/master/src/consensus/upgrades.cpp))
It's convenient to add these to the configuration file, for example:
```
$ cat <<EOF >>/tmp/regtest-datadir/zcash.conf
nuparams=76b809bb:1
nuparams=f5b9230b:5
EOF
```
(Alternatively, you can specify these on the `zcashd` command line.)
You need not activate every upgrade explicitly. The example activates Sapling
(branchID 76b809bb) at height 1; activating Sapling implies activating Overwinter, so this
is done automatically. Similarly, activating Heartwood at height 5
also simultaneously activates Blossom. Since no later upgrades are specified, none
of them will activate, regardless of height reached.

**IMPORTANT**: if you change the network upgrade heights from one
test run to the next, it's almost always necessary to start fresh
by removing the data directory, otherwise you'll encounter strange errors.

You can see which network upgrades are currently active and which are pending
(using the above `nuparams` settings as an example):
```
$ src/zcash-cli -datadir=/tmp/regtest-datadir generate 2
$ src/zcash-cli -datadir=/tmp/regtest-datadir getblockchaininfo
{
  "blocks": 2,
  (...)
    "upgrades": {
    "5ba81b19": {
      "name": "Overwinter",
      "activationheight": 1,
      "status": "active",
      "info": "See https://z.cash/upgrade/overwinter/ for details."
    },
    "76b809bb": {
      "name": "Sapling",
      "activationheight": 1,
      "status": "active",
      "info": "See https://z.cash/upgrade/sapling/ for details."
    },
    "2bb40e60": {
      "name": "Blossom",
      "activationheight": 5,
      "status": "pending",
      "info": "See https://z.cash/upgrade/blossom/ for details."
    },
    "f5b9230b": {
      "name": "Heartwood",
      "activationheight": 5,
      "status": "pending",
      "info": "See https://z.cash/upgrade/heartwood/ for details."
    }
  },
  (...)
}
```

## Manual testing within an RPC (Python) test run

It is often useful, either when debugging an issue or simply when you want to put
the node into a certain state, to use the RPC test framework to produce the desired
state and then be able to manually interrogate and modify that state using `zcash-cli` 
to execute RPC calls. An easy way to do that is as follows:

Add the line `import time; time.sleep(999999)` (the units are seconds) somewhere
within an RPC test to pause its execution at that point. Start the test, and then:

```
$ ps alx | grep zcashd
0  1000   96247   96246  20   0 1426304 123952 futex_ SLl+ pts/12   0:18 /g/zcash/src/zcashd -datadir=/tmp/test9d907s8a/96246/node0 -keypool=1 -discover=0 -rest -nuparams=5ba81b19:1 -nuparams=76b809bb:1 -debug=mempool -mempooltxcostlimit=8000
0  1000   96274   96246  20   0 744092 85568 -      RLl+ pts/12     0:05 /g/zcash/src/zcashd -datadir=/tmp/test9d907s8a/96246/node1 -keypool=1 -discover=0 -rest -nuparams=5ba81b19:1 -nuparams=76b809bb:1 -debug=mempool -mempooltxcostlimit=8000
$ 
```
Now you can interact with the running test node by copy-and-pasting its
`-datadir` argument, for example:

```
$ src/zcash-cli -datadir=/tmp/test9d907s8a/96246/node0 getblockchaininfo
```
(The other `zcashd` command-line arguments are generally not needed by
`zcash-cli`.) You can see the running node's debug log file:
```
$ cat /tmp/test9d907s8a/96246/node0/regtest/debug.log
```
or look at its configuration file.
```
$ cat /tmp/test9d907s8a/96246/node0/zcash.conf
```
In this way, the RPC test framework can teach us more about running `regtest` nodes.
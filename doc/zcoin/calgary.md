# Calgary Design

## Motivation
Calgary is a reimplementation of Zerocash in Bitcoin meant to address a number of shortcomings and design flaws in the original academic implementation. This gives us an opportunity to rework the implementation from scratch, 
setting out rigorous guidelines for how we interact with upstream code.

### Bridging implementation gaps
The original implementation carried Protect and Pour operations through the transaction inputs. This required a number of hacks. Transaction inputs that carried Pours, for example, had no actual `CTxOut` to spend, so a dummy 
"always_spendable" transaction was created. The scripting system (opcodes in particular) had to be changed to store the zerocash operation metadata. Changes to support things like intermediate Pours or input value to the circuit 
had unclear interactions with the transaction system.

The clear advantage of the original implementation was avoiding structural changes to `CTransaction`. However, these are clearly necessary, not least because versioning semantics must be laid out in a sensible way.

## Goals
* We should take this opportunity to understand the implementation better, and build on top of a more recent version of Bitcoin. Upstream has made a number of changes to critical components and has begun to refactor 
consensus-critical code into a `libconsensus` library.
* We should rigorously practice our [design policy](https://github.com/Electric-Coin-Company/zerocashd/wiki/design). This includes avoiding changes to upstream's scripting system wherever possible, and only modifying structures 
with proper versioning plans.
* We should strive to preserve the semantics of our private alpha implementation such that the `zc-raw-*` RPC commands still work properly.

## Implementation strategy

### CTransaction

The heart of zerocash modifications is in `CTransaction`. The current layout is as follows:

```
{
	    int32_t nVersion;
	    const std::vector<CTxIn> vin;
	    const std::vector<CTxOut> vout;
	    const uint32_t nLockTime;
}
```

We would like to place Protect and Pour operations *outside* of the `vin` and `vout` fields. There are a number of concerns surrounding such a modification

#### Versioning

`CTransaction`'s `nVersion` field is used by upstream to make changes to Bitcoin's transaction system. It's currently at version `3`. Ideally we would like to "track" upstream transaction system changes. However, we are making 
changes to the transaction structure which *require* us to interact with `nVersion` to avoid breaking tests. We also need to discriminate transactions between protects, pours or new kinds of zerocash operations in the future, 
meaning that some versioning scheme is needed.
##### Approach 1: Increment `nVersion` and carefully track upstream changes
This effectively punts long-term decisions about tracking upstream `CTransaction` changes to when those version changes are actually made. This is a nice solution, because it's unclear when or whether upstream Bitcoin will 
increment this version, and it's not clear how upstream will use `nVersion`. (Will they use it purely as an incrementing version, or place bitflags in it?) Additionally, the version number has to be changed anyway, and if in the 
future we cannot use `nVersion` without messy interactions with upstream code, we can *then* implement a versioning scheme with as much fuss as now.

As part of this approach, some meaningful work can be done to introduce upstream-downstream versioning semantics. [insert nathan's idea here]
##### Approach 2: Increment `nVersion` and place small versioning data into bitflags.
This is a variant of approach 1 which assumes upstream will not place bitflags into higher-order bits of `nVersion`. The benefit is that `nVersion` changes in upstream (and the tests that introduce them) *never* intrude on our 
versioning changes. That is, if upstream increments to version `4` and adds tests for version 4 transactions they *should* behave the same, as our bits indicating zerocash structure changes will not be set.

#### Structure of zerocash operations

##### Approach 1: Minimal pour/protect behaviors

Add a new field `zcType` which indicates either `NOP`, `PROTECT` or `POUR`.

* `NOP` is a traditional transaction and has no interaction with the zerocash system.
* `PROTECT` implies another field is added (`zcProtect`) of type `libzerocash::ProtectTransaction`.
* `POUR` implies new fields are added (`zcPourKey`, `zcPourSig`, `zcPour`,`zcAnchor`). `zcPourKey` is a one-time key used to sign the transaction (`SIGHASH_ALL`-style) alongside changes to the scripting system which avoid hashing 
the `zcPourSig` similar to how transaction input `scriptSig`s are not serialized. `zcPour` is of type `libzerocash::PourTransaction`.

The `zcAnchor` field of the pour must reference a merkle root, either by specifying a block hash to obtain the merkle root from (assuming the existence of a mapping), or a merkle root which validators ensure exists within the 
active chain.

##### Approach 2: Sum types
A variant of approach 1. This places the above invariants and discriminants into a single field `zcOperation` of type `ZCOperation` and includes the necessary subfields, serialization routines and `SIGHASH_*` interactions.

##### Approach 3: Sum types and vectors
A variant of approach 2. Add a new field `zcOperations` of type `std::vector<ZCOperation>` which permits multiple operations. The `NOP` case is represented by this vector being size `0`. Presumably some kind of size limit is 
enforced. Each `ZCOperation` cannot be independently verified as the verification depends on the previous `ZCOperation`'s interactions. (As an example, two Protects should not be able to protect the same value from the input!)

### CBlock

In the previous design, blocks stored the root of the commitment merkle tree. This isn't really necessary as the commitment tree, like the UTXO, is derived from the transactions in the block.


# Out of Scope

This design does *not* address some crucial issues. Here are the ones we know about, currently:

## Circuit Changes

For this design we're assuming no change to the Pour circuit. In followup design work we expect this to change, as we may choose to alter this circuit to take a public input symmetric with the public output, to use a different hash, to have different numbers of inputs/outputs, etc...

## Block header changes

For this design we'll ignore chagnes to the block header, and in fact assume the block header structure is unaltered. (This implies the commitments and spent serials are tracked-per-block by all full nodes, with no commitment to those structures in the block headers.)

## Systemic Incentives for Fungibility

All other things being equal, *if* cleartext transactions tend to be lower cost for users, then the whole system may migrate to mostly cleartext. Anonymity set size is a system-wide property, and the currency is only generally fungible if the vast majority of coins are Protected.

We may try to incentivize fungibility system-wide by specific consensus rule constraints, or default fee policy, or other mechanisms.

## relaypriority

Related to the last issue, we may alter the "relay priority" stuff. It sounds like bitcoin-core plans to phase it out. For this design we'll just follow whatever upstream does and "make it work".

## Wallet Fee or Merge/Split Behavior

In the long run, we need to be careful when considering wallet designs in how fees are chosen and when a wallet merges or splits buckets. These decisions have privacy and security impacts. In this design, we assume those decisions are completely "at a higher stack layer" and have no bearing on this design's CTransaction changes.

## PoW

This doesn't interact with PoW in any way of which we're currently aware.

## Other?

And probably a thousand other things...

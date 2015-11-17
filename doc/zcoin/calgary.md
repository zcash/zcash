# Calgary Design

## Motivation
Calgary is a reimplementation of Zerocash in Bitcoin meant to address a number of shortcomings and design flaws in the original academic implementation. This gives us an opportunity to rework the implementation from scratch, setting out rigorous guidelines for how we interact with upstream code.

### Bridging implementation gaps
The original implementation carried Protect and Pour operations through the transaction inputs. This required a number of hacks. Transaction inputs that carried Pours, for example, had no actual `CTxOut` to spend, so a dummy "always_spendable" transaction was created. The scripting system (opcodes in particular) had to be changed to store the zerocash operation metadata. Changes to support things like intermediate Pours or input value to the circuit had unclear interactions with the transaction system.

The obvious advantage of the original implementation was avoiding structural changes to `CTransaction`. However, these are necessary, not least because versioning semantics must be laid out in a sensible way.

## Goals
* We should take this opportunity to understand the implementation better, and build on top of a more recent version of Bitcoin. Upstream has made a number of changes to critical components and has begun to refactor consensus-critical code into a `libconsensus` library.
* We should rigorously practice our [design policy](https://github.com/Electric-Coin-Company/zerocashd/wiki/design). This includes avoiding changes to upstream's scripting system wherever possible, and only modifying structures with proper versioning plans.
* We should strive to preserve the semantics of our private alpha implementation such that the `zc-raw-*` RPC commands still work properly.

## Scope

* **Chained pours** (#121) mean that we should anticipate multiple pours in a transaction, specifically because the pours may require commitments from previous pours.
* **Symmetric pours** (#338) mean that instead of separate Protect / Pour operations, a *single* Pour operation exists which takes a `vpub_in` and `vpub_out`, unifying the two operations. This requires a circuit change.
* **Versioning semantics** (#114) require us to avoid breaking upstream tests whenever possible. We need to anticipate both changes to our own structures after launch to support new features (such as circuit changes, see #152) and potential changes to upstream transaction structures we will eventually need to rebase on top of.
* **Cryptographic binding of pours** (#336) is necessary to ensure that (in the most common situation) it is not possible to move a pour from one transaction to another, or replace pours in a transaction without the authorization of its inputs.

### Not in Scope

#### PoW and other block header changes

It should not be necessary to make any block header changes yet in the design. We anticipate to change the PoW scheme, which may affect how nonces appear in coinbases. However, this doesn't appear to have a significant affect on the overall design, so we do not specify its implications here.

#### Systemic incentives for fungibility

It will be necessary to make a number of modifications to the fee calculation and priority system, because pours will introduce larger transactions. We must be careful to avoid a "tragedy of the commons" scenario which would make protected transactions more expensive to process, producing adverse incentives for the network.

Upstream bitcoin appears to be phasing out `relaypriority` and other priority systems.

#### Anonymity set partitioning (#425)

Any place in our design where multiple structural decisions can be made, especially by wallet software, allows not only particular wallet software to be identified but potentially users as well. This is most pronounced when determining how to split and merge buckets. (Depending on when and how the buckets are merged, it may be possible to identify users.)

Additionally, the merkle root anchor indicates to others your "view" of the network state, which could be used to identify users.

## Design

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

#### Versioning

In our design, we will increment the latest `nVersion` for transactions, adding new fields to the end to encapsulate our zerocash operations. Our fields must anticipate the case that no zerocash operations occur, such as in traditional or "purely cleartext" transactions.

##### Alternative: Use bitflags to indicate the presence of zerocash fields
In this alternative, we use bitflags in the `nVersion` field to indicate the presence of zerocash fields at the end. This would allow us to track upstream CTransaction changes seamlessly, *but* would indefinitely require that upstream `nVersion` bits are available to us for this purpose. Additionally, the absence of these bitflags would conflict in purpose with an empty vector of `ZCOperation` structures as outlined later.

#### ZCOperation

We add an additional field to the end of a `CTransaction`, a vector of `ZCOperation` structures called `vZCop`. This vector is merely empty in the case of traditional transactions. Operations are allowed to reference the (commitment tree) merkle root produced by a previous operation or block for verification purposes.

In the original academic implementation, an ephemeral ECDSA keypair is generated and used to sign the rest of the transaction, binding it to the Pour as the public key is provided as an input to the prover/verifier. We can reuse Bitcoin's scripting system to abstract this behavior further, allowing for more flexible binding behaviors between pours and the rest of the transaction. As an example of a future use of this flexibility, crowdfunding pours would be possible by allowing users to bind their pours to nothing else in the transaction.

The ultimate structure of a `ZCOperation` is as follows:

```
struct ZCOperation {
	anchor, // merkle root this operation is anchored to
	scriptPubKey, // CScript which is hashed and provided as an input to the snark verifier/prover
	scriptSig, // CScript which is verified against scriptPubKey
	vpub_in, // the value to draw from the public value pool
	vpub_out, // the value to provide to the public value pool
	pour // libzerocash::PourTransaction (modified slightly)
}
```

The `CTransactionSignatureSerializer` (and perhaps other components) will need to be modified so that the inputs are cryptographically bound to the `ZCOperation`s.

##### Alternative: `SIGHASH_ALL` binding
In this alternative, instead of using `CScript`s to offer flexible binding semantics to users, we force `SIGHASH_ALL`-style signature checking on the transaction. This is safer if we discover that scripts in this context have strange interactions that cannot be bridged, or that malleability issues are unavoidable.

### CBlock

In the previous design, blocks stored the root of the commitment merkle tree. This isn't really necessary as the commitment tree, like the UTXO, is derived from the transactions in the block. It will likely be necessary to retain a mapping between merkle roots and block hashes.

## Implementation strategy

Various parts of this design can be implemented concurrently and iteratively.

### Stage 1: Foundations

1. `CTransaction` should be modified so that additional fields are supported.
2. An additional vector of `ZCOperation`s should be added. Only allow size `0` and size `1` for now, to avoid implementation of chained pours in the mean time.
3. `ZCOperation` should initially encapsulate Protect and Pour operations before unification is implemented in the circuit.
4. Implement a simple `OP_RETURN`-style binding scheme for the `CScripts`.

### Stage 2: Concurrent tasks
Each of the following tasks to complete the redesign can be done independently.

* `CScript` scheme to enforce cryptographic binding of the transaction to the pour.
* Chained pours which allow pours to reference merkle roots produced by other pours.
* Circuit unification of Protect/Pour semantics would allow us to refactor the `ZCOperation` structure.
* A zerocash-specific versioning field can be added, along with upstream interaction semantics.

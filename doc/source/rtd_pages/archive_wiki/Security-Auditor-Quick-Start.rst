This page provides information for security auditors who want to get
started looking for bugs in Zcash but are faced with the task of
figuring out which designs and code are most critical to review.

General Description
-------------------

The protocol began with `Zerocoin <http://zerocoin.org/>`__ which
evolved into `Zerocash <http://zerocash-project.org/>`__. Zerocash is
specified in an `56-page extended
paper <http://zerocash-project.org/media/pdf/zerocash-extended-20140518.pdf>`__.
The Zcash protocol, which differs from Zerocash in important ways, is
specified in the `Zcash Protocol
Specification <https://github.com/zcash/zips/blob/master/protocol/protocol.pdf>`__.

Zcash is a fork of `Bitcoin Core <https://github.com/bitcoin/bitcoin>`__
which descends from Satoshi's original implementation of Bitcoin, and is
the most popular full node client. We are forked off of a release tag,
``v0.11.2``. Our development happens on the
[STRIKEOUT:``zc.v0.11.2.latest``]\ ``master`` branch.

The code is C++ (compiled with C++11 support), networking,
multi-threaded. Various BOOST library features are used throughout the
code.

Our core cryptographic dependency containing the zero-knowledge proving
system is `libsnark <https://github.com/scipr-lab/libsnark>`__. We
currently do not track the latest upstream branch; we use [STRIKEOUT:git
revision ``69f312f149cc4bd8def8e2fed26a7941ff41251d``] the revision
specified in
https://github.com/zcash/zcash/blob/master/depends/packages/libsnark.mk
.

Past and Currently-Open Security Issues
---------------------------------------

If you like to take inspiration from previously-discovered security
issues, or look at our ever-growing list of "there might be a problem
here" tickets, look at our SECURITY label:

https://github.com/zcash/zcash/labels/SECURITY

Also see libsnark's issue tracker:

https://github.com/scipr-lab/libsnark/issues

List of Consensus Changes
-------------------------

Here's a (currently incomplete) list of the changes we've made to
upstream Bitcoin's consensus rules:

-  Implemented the Zcash protocol:

   -  CTransactions can now contain zero or more JoinSplit operations
      (zero-knowledge transactions).
   -  Global balance properties are now enforced by checking
      zero-knowledge proofs when necessary.
   -  The meaning of what a "valid transaction" is has changed, e.g.
      it's now possible to have 0-input transactions if they contain a
      JoinSplit.
   -  JoinSplit operations must be tied to a valid Merkle tree root, so
      reordering transactions is more difficult, whereas upstream
      transactions can be reordered easily. We no longer interact with
      the mapOrphans system.
   -  Full nodes have to track new data structures (the note commitment
      incremental Merkle tree and the spent serials map).

-  We've switched the Proof of Work to Equihash.
-  Changed the difficulty adjustment algorithm to one based on
   DigiShield v3/v4.
-  Mining slow start.
-  Founder's reward.
-  Added a reserved field to the block header.
-  All signature hash types (flags) now have their signatures cover the
   JoinSplits in the transaction.
-  Checking of SIGHASH\_SINGLE transactions returns proper exceptions,
   upstream bug not replicated.
-  Coinbase transactions can only be spent with a JoinSplit, providing
   "privacy by default."
-  Collapsed all of the blockheight- and time-triggered soft forking
   rules to apply to all blocks.
-  Increased the block size to 2MB.
-  Decreased the target block interval to 2.5 minutes.

Non-consensus Changes
---------------------

-  Decreased MAX\_HEADERS\_RESULTS (the number of block headers that are
   sent in a single protocol message) to 160.
-  Set relaypriority to false, disabling the transaction priority system
   by default.
-  We disabled network bootstrapping; going to have to enable this
   again.
-  Changed the keys for the alert broadcasting system.
-  Changed the default port numbers (so that they aren't the same as
   upstream Bitcoin's).
-  Network protocol magic numbers changed to separate it from the
   bitcoin network.
-  Planned: A better system for distributing the zkSNARK proving and
   verifying keys (for now: downloaded off of S3).

List of Dependencies
--------------------

All of our dependencies are specified by files in ``depends/packages``.
See those files to get a complete and current list of dependencies and
their versions.

Some dependencies of special note to security auditors are:

-  libsnark, which implements the zkSNARK zero-knowledge proving system.
-  libsodium, which we use for BLAKE2, a random number generator, and
   elliptic-curve encryption for some parts of the Zcash protocol.
-  openssl

libsnark
~~~~~~~~

Zcash only touches part of libsnark, as libsnark includes multiple
implementations of the same things, and extra circuit gadgets that we
don't use.

**(Note that libsnark itself has dependencies).**

The parts we certainly DO use are the following:

-  ``src/gadgetlib1``.
-  ``src/common``.
-  ``src/algebra``.
-  ``zk_proof_systems/ppzksnark/r1cs_ppzksnark``
-  ``libsnark/gadgetlib1/gadgets/hashes/sha256``
-  ``gadgetlib1/gadgets/merkle_tree``
-  ``curves/alt_bn128``
-  (possibly in the future) ``curves/bn128``

The parts we certainly DO NOT use are the following:

-  ``src/gadgetlib2``.
-  ``src/gadgetlib1/gadgets/{cpu_checkers, delegated_ra_memory, hashes/knapsack, verifiers, pairing}``.
-  ``src/zk_proof_systems/{pcd, ppzkadsnark, zksnark}``.
-  ``src/algebra/curves/{edwards, mnt}``.

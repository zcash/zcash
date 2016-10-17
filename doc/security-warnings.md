Security Warnings
====================

Security Audit
--------------

Zcash has been subjected to a formal third-party security review. For high priority security announcements, check https://z.cash.

x86-64 Linux Only
-----------------------

There are [known bugs](https://github.com/scipr-lab/libsnark/issues/26) which
make proving keys generated on 64-bit systems unusable on 32-bit and big-endian
systems. It's unclear if a warning will be issued in this case, or if the
proving system will be silently compromised.

Side-Channel Attacks
--------------------

This implementation of Zcash is not resistant to side-channel attacks. You
should assume (even unprivileged) users who are running on the hardware, or who
are physically near the hardware, that your `zcashd` process is running on will
be able to:

- Determine the values of your secret spending keys, as well as which notes you
  are spending, by observing cache side-channels as you perform a JoinSplit
  operation. This is due to probable side-channel leakage in the libsnark
  proving machinery.

- Determine which notes you own by observing cache side-channel information
  leakage from the incremental witnesses as they are updated with new notes.

- Determine which notes you own by observing the trial decryption process of
  each note ciphertext on the blockchain.

You should ensure no other users have the ability to execute code (even
unprivileged) on the hardware your `zcashd` process runs on until these
vulnerabilities are fully analyzed and fixed.

REST Interface
--------------

The REST interface is a feature inherited from upstream Bitcoin.  By default,
it is disabled. We do not recommend you enable it until it has undergone a
security review.

Block Chain Reorganization: Major Differences
---------------------------------------------------

Users should be aware of new behavior in Zcash that differs significantly from Bitcoin: in the case of a block chain reorganization, Bitcoin's coinbase maturity rule helps to ensure that any reorganization shorter than the maturity interval will not invalidate any of the rolled-back transactions. Zcash keeps Bitcoin's 100-block maturity interval for generation transactions, but because JoinSplits must be anchored within a block, this provides more limited protection against transactions becoming invalidated. In the case of a block chain reorganization for Zcash, all JoinSplits which were anchored within the reorganization interval and any transactions that depend on them will become invalid, rolling back transactions and reverting funds to the original owner. The transaction rebroadcast mechanism inherited from Bitcoin will not successfully rebroadcast transactions depending on invalidated JoinSplits if the anchor needs to change. The creator of an invalidated JoinSplit, as well as the creators of all transactions dependent on it, must rebroadcast the transactions themselves.

Receivers of funds from a JoinSplit can mitigate the risk of relying on funds received from transactions that may be rolled back by using a higher minconf (minimum number of confirmations).

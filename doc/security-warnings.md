Security Warnings
====================

Security Audit
--------------

Zcash has not yet been subjected to a formal third-party security review. This
section will be updated with links to security audit reports in the future.

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

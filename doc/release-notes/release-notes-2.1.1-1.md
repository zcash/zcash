Notable changes
===============

This release fixes a security issue described at
https://z.cash/support/security/announcements/security-announcement-2020-02-06/ .

This release also adds a `-maxtimeadjustment` option to set the maximum time, in
seconds, by which the node's clock can be adjusted based on the clocks of its
peer nodes. This option defaults to 0, meaning that no such adjustment is performed.
This is a change from the previous behaviour, which was to adjust the clock by up
to 70 minutes forward or backward. The maximum setting for this option is now
25 minutes (1500 seconds).

Fix for incorrect banning of nodes during syncing
-------------------------------------------------
After activation of the Blossom network upgrade, a node that is syncing the
block chain from before Blossom would incorrectly ban peers that send it a
Blossom transaction. This resulted in slower and less reliable syncing (#4283).

Changelog
=========

Daira Hopwood (10):
      Move check for block times that are too far ahead of adjusted time, to ContextualCheckBlock.
      Improve messages for timestamp rules.
      Add constant for how far a block timestamp can be ahead of adjusted time. Loosely based on https://github.com/bitcoin/bitcoin/commit/e57a1fd8999800b3fc744d45bb96354cae294032
      Soft fork: restrict block timestamps to be no more than 90 minutes after the MTP of the previous block.
      Adjust the miner to satisfy consensus regarding future timestamps relative to median-time-past.
      Enable future timestamp soft fork at varying heights according to network.
      Cosmetic: brace style in ContextualCheckBlockHeader.
      Add -maxtimeadjustment with default of 0 instead of the 4200 seconds used in Bitcoin Core.
      Fix ContextualCheckBlock test (the ban score should be 100 since these are mined transactions).
      Add string argument to static_asserts to satisfy C++11.

Jack Grigg (2):
      test: Update RPC test cache generation to handle new consensus rule
      Apply a consistent ban policy within ContextualCheckTransaction

Sean Bowe (3):
      Release notes for vulnerability and -maxtimeadjustment option.
      make-release.py: Versioning changes for 2.1.1-1.
      make-release.py: Updated manpages for 2.1.1-1.


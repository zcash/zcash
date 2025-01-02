Changelog
=========

Daira-Emma Hopwood (6):
      Funding streams: Rename INSUFFICIENT_ADDRESSES to INSUFFICIENT_RECIPIENTS.
      sapling::Builder: add funding stream outputs in a more predictable order. Previously it was deterministic, but depended on details of the `operator<` implementations of `FundingStreamRecipient`, `SaplingPaymentAddress`, and `CScript`. Now it is just the same order as in `vFundingStreams`.
      Simplify `GetActiveFundingStreamElements`.
      gtest/test_checkblock.cpp: fix a bug in MockCValidationState that was causing a test warning.
      Miner tests: correct a comment, and add another similar comment where it applies. Doc-only.
      Update Daira-Emma Hopwood's gpg keys.

Jack Grigg (4):
      qa: Fix line wrapping in `show_help`
      qa: Postpone all dependency updates until after 6.1.0
      make-release.py: Versioning changes for 6.1.0.
      make-release.py: Updated manpages for 6.1.0.

Larry Ruane (1):
      move Lockbox value logging behind category valuepool


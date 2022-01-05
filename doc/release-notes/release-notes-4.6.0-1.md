`getblocktemplate` regression fix
=================================

We added support for the NU5 consensus rules in v4.5.0, which alters the
block header to contain a `hashBlockCommitments` value instead of the
chain history root. However, the output of `getblocktemplate` wasn't
returning this value; once NU5 activated, the `blockcommitmentshash`
field was being set to "null" (all-zeroes).

In v4.6.0 we added full NU5 support to `getblocktemplate`, by adding a
`defaultroots` field that gave default values for `hashBlockCommitments`
and the components required to derive it. However, in doing so we
introduced a regression in the (now-deprecated) legacy fields, where
prior to NU5 activation they contained nonsense.

This release fixes the output of `getblocktemplate` to have the intended
semantics for all fields:

- The `blockcommitmentshash` and `authdataroot` fields in `defaultroots`
  are now omitted from block templates for heights before NU5 activation.

- The legacy fields now always contain the default value to be placed
  into the block header (regaining their previous semantics).

Changelog
=========

Jack Grigg (3):
      rpc: Fix regression in getblocktemplate output
      make-release.py: Versioning changes for 4.6.0-1.
      make-release.py: Updated manpages for 4.6.0-1.

Larry Ruane (3):
      assert that the return value of submitblock is None
      test: check getblocktemplate output before and after NU5
      test: Fix ZIP 244 implementation


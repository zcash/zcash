(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Reduce download traffic
-----------------------

We have made several changes to reduce the amount of data downloaded by `zcashd`
during initial block download (IBD):

- Significant time and bandwidth is spent in issuing `getheaders` P2P requests.
  This results in noticeable bandwidth usage due to the large size of Zcash
  block headers.

  We now eliminate redundant requests in cases where we already know the last
  header in the message. This optimization is enabled by default, but can be
  disabled by setting the config option `-no-optimize-getheaders`.

- Transactions in the mempool are no longer downloaded during IBD (`zcashd` will
  only request block data).

Reduce upload traffic
---------------------

A major part of the outbound traffic is caused by serving historic blocks to
other nodes in initial block download state.

It is now possible to reduce the total upload traffic via the `-maxuploadtarget`
parameter. This is *not* a hard limit but a threshold to minimize the outbound
traffic. When the limit is about to be reached, the uploaded data is cut by not
serving historic blocks (blocks older than one week).
Moreover, any SPV peer is disconnected when they request a filtered block.

This option can be specified in MiB per day and is turned off by default
(`-maxuploadtarget=0`).
The recommended minimum is 1152 * MAX_BLOCK_SIZE (currently 2304MB) per day.

Whitelisted peers will never be disconnected, although their traffic counts for
calculating the target.

A more detailed documentation about keeping traffic low can be found in
[/doc/reduce-traffic.md](/doc/reduce-traffic.md).

`libzcashconsensus` replaced by `libzcash_script`
-------------------------------------------------

The `libzcashconsensus` library inherited from upstream has been unusable since
the Overwinter network upgrade in 2018. We made changes to signature digests
similar to those made in Bitcoin's SegWit, which required additional per-input
data that could not be added to the existing APIs without breaking backwards
compatibility.

Additionally, it has become increasingly inaccurately named; it only covers
(Zcash's subset of) the Bitcoin scripting system, and not the myriad of other
consensus changes: in particular, Zcash's shielded pools.

We have now renamed the library to `libzcash_script`, and reworked it to instead
focus on transparent script verification:

- The script verification APIs are altered to take `consensusBranchId` and
  `amount` fields.
- New precomputing APIs have been added that enable multiple transparent inputs
  on a single transaction to be verified more efficiently.
- Equihash has been removed from the compiled library. The canonical Equihash
  validator is the [`equihash` Rust crate](https://crates.io/crates/equihash)
  since v3.1.0.

The C++ library can be built by compiling `zcashd` with the environment variable
`CONFIGURE_FLAGS=--with-libs`. It is also wrapped as the
[`zcash_script` Rust crate](https://crates.io/crates/zcash_script)
(maintained by the Zcash Foundation for use in `zebrad`).

Other P2P Changes
-----------------

The list of banned peers is now stored on disk rather than in memory. Restarting
`zcashd` will no longer clear out the list of banned peers; instead the
`clearbanned` RPC call can be used to manually clear the list. The `setban` RPC
call can also be used to manually ban or unban a peer.

Build system updates
--------------------

- We now build with Clang 11 and Rust 1.49.
- We have downgraded Boost to 1.74 to mitigate `statx`-related breakage in some
  container environments.

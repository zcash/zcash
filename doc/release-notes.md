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
[/doc/reducetraffic.md](/doc/reducetraffic.md).

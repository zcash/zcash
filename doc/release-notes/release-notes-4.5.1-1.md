Notable changes
===============

The `listaddresses` endpoint has been added to the RPC API. This method
allows the caller to obtain addresses managed by the wallet, grouped
by the source of the address, including both those addresses generated
by the wallet and those associated with imported viewing or spending
keys. This provides functionality that replaces and subsumes the 
previously-removed `getaddressesbyaccount` method.

Changelog
=========

Jack Grigg (6):
      Mark v5 transaction format as standard for NU5
      Fix comment
      cargo update
      depends: Postpone dependency updates
      make-release.py: Versioning changes for 4.5.1-1.
      make-release.py: Updated manpages for 4.5.1-1.

Kris Nuttycombe (11):
      Add `listaddresses` RPC method.
      Categorize listaddresses result by source type.
      Correctly handle imported Sapling addresses
      Apply suggestions from code review
      Apply suggestions from code review
      Group legacy_hdseed Sapling addresses by account ID.
      Update release notes for v4.5.1-1 to reflect the addition of `listaddresses`
      Include `ImportedWatchOnly` as a PaymentAddressSource result.
      Add listaddresses check to wallet_addresses.py
      Consistently group Sapling addresses by IVK for every source.
      Use lowerCamelCase for listaddresses JSON


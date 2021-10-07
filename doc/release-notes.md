(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

The `listaddresses` endpoint has been added to the RPC API. This method
allows the caller to obtain addresses managed by the wallet, grouped
by the source of the address, including both those addresses generated
by the wallet and those associated with imported viewing or spending
keys. This provides functionality that replaces and subsumes the 
previously-removed `getaddressesbyaccount` method.

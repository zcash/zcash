(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Other issues
============

Revoked key error when upgrading on Debian
------------------------------------------

If you see the following error when updating to a new version of zcashd:

`The following signatures were invalid: REVKEYSIG AEFD26F966E279CD`

Remove the key marked as revoked:

`sudo apt-key del AEFD26F966E279CD`

Then retrieve the updated key:

`wget -qO - https://apt.z.cash/zcash.asc | sudo apt-key add -`

Then update the package lists:

`sudo apt-get update`

[Issue](https://github.com/zcash/zcash/issues/3612)

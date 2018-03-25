# Wallet Backup Instructions

## Overview

Backing up your Zcash private keys is the best way to be proactive about preventing loss of access to your ZEC.

Problems resulting from bugs in the code, user error, device failure, etc. may lead to losing access to your wallet (and as a result, the private keys of addresses which are required to spend from them).

No matter what the cause of a corrupted or lost wallet could be, we highly recommend all users backup on a regular basis. Anytime a new address in the wallet is generated, we recommending making a new backup so all private keys for addresses in your wallet are safe.

Note that a backup is a duplicate of data needed to spend ZEC so where you keep your backup(s) is another important consideration. You should not store backups where they would be equally or increasingly susceptible to loss or theft. 

## Instructions for backing up your wallet and/or private keys

These instructions are specific for the officially supported Zcash Linux client. For backing up with third-party wallets, please consult with user guides or support channels provided for those services.

There are multiple ways to make sure you have at least one other copy of the private keys needed to spend your ZEC and view your shielded ZEC.

For all methods, you will need to include an export directory setting in your config file (`zcash.conf` located in the data directory which is `~/.zcash/` unless it's been overridden with `datadir=` setting):

`exportdir=path/to/chosen/export/directory`

You may chose any directory within the home directory as the location for export & backup files. If the directory doesn't exist, it will be created.

Note that zcashd will need to be stopped and restarted for edits in the config file to take effect. 

### Using `backupwallet`

To create a backup of your wallet, use:

`zcash-cli backupwallet <nameofbackup>`.

The backup will be an exact copy of the current state of your wallet.dat file stored in the export directory you specified in the config file. The file path will also be returned.

If you generate a new Zcash address, it will not be reflected in the backup file.

If your original `wallet.dat` file becomes inaccessible for whatever reason, you can use your backup by copying it into your data directory and renaming the copy to `wallet.dat`.

### Using `z_exportwallet` & `z_importwallet`

If you prefer to have an export of your private keys in human readable format, you can use:

`zcash-cli z_exportwallet <nameofbackup>`

This will generate a file in the export directory listing all transparent and shielded private keys with their associated public addresses. The file path will be returned in the command line.

To import keys into a wallet which were previously exported to a file, use:

`zcash-cli z_importwallet <path/to/exportdir/nameofbackup>`

### Using `z_exportkey`, `z_importkey`, `dumpprivkey` & `importprivkey`

If you prefer to export a single private key for a shielded address, you can use:

`zcash-cli z_exportkey <z-address>`

This will return the private key and will not create a new file.

For exporting a single private key for a transparent address, you can use the command inherited from Bitcoin:

`zcash-cli dumpprivkey <t-address>`

This will return the private key and will not create a new file.

To import a private key for a shielded address, use:

`zcash-cli z_importkey <z-priv-key>`

This will add the key to your wallet and rescan the wallet for associated transactions if it is not already part of the wallet.

The rescanning process can take a few minutes for a new private key. To skip it, instead use:

`zcash-cli z_importkey <z-private-key> no`

For other instructions on fine-tuning the wallet rescan, see the command's help documentation:

`zcash-cli help z_importkey`

To import a private key for a transparent address, use:

`zcash-cli importprivkey <t-priv-key>`

This has the same functionality as `z_importkey` but works with transparent addresses.

See the command's help documentation for instructions on fine-tuning the wallet rescan:

`zcash-cli help importprivkey`

### Using `dumpwallet`

This command inherited from Bitcoin is deprecated. It will export private keys in a similar fashion as `z_exportwallet` but only for transparent addresses.
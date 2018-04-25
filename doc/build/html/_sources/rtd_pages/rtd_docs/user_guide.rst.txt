.. _User-Guide:

User Guide
==========

About
-----

This repository is a fork of `Bitcoin Core`_ which contains protocol
changes to support the `Zerocash protocol`_. This implements the Zcash
cryptocurrency, which maintains a separate ledger from the Bitcoin
network, for several reasons, the most immediate of which is that the
consensus protocol is different.

Getting Started
---------------

Welcome! This guide is intended to get you running on the official Zcash network. To ensure your Zcash client behaves gracefully throughout the setup process, please check your system meets the following requirements:

	- 64-bit Linux OS
	- 64-bit Processor
	- 3GB of free RAM
	- 10GB of free Disk (*the size of the block chain increases over time*)

.. note:: Currently we only officially support Linux (Debian), but we are actively investigating development for other operating systems and platforms(e.g. OSX, Ubuntu, Windows, Fedora). 


Please let us know if you run into snags. We plan to make it less memory/CPU intensive and support more architectures and operating systems in the future.

If you are installing Zcash for the first time, please skip to the Important Teminology section. Otherwise, the below upgrading section will provide information to update your current Zcash environment.

Upgrading?
----------

If you're on a Debian-based distribution, you can follow the [Debian instructions](https://github.com/zcash/zcash/wiki/Debian-binary-packages) to install zcash on your system. Otherwise, you can update your local snapshot of our code:

.. code-block:: bash

   git fetch origin
   git checkout v1.0.15
   ./zcutil/fetch-params.sh
   ./zcutil/build.sh -j$(nproc)

.. note:: If you don't have `nproc`, then substitute the number of cores on your system. If the build runs out of memory, try again without the `-j` argument, i.e. just `./zcutil/build.sh`. If you are upgrading from testnet, make sure that your ``~/.zcash`` directory contains only ``zcash.conf`` to start with, and that your `~/.zcash/zcash.conf` does not contain `testnet=1` or `addnode=testnet.z.cash`. If the build fails, move aside your ``zcash`` directory and try again by following the instructions in the [Compile it yourself](#or-compile-it-yourself) section below.

Important Terminology
---------------------

Zcash supports two different kinds of addresses, a _z-addr_ (which begins with a **z**) is an address that uses zero-knowledge proofs and other cryptography to protect user privacy. There are also _t-addrs_ (which begin with a **t**) that are similar to Bitcoin's addresses.

The interfaces are a commandline client (`zcash-cli`) and a Remote Procedure Call (RPC) interface, which is documented here:

https://github.com/zcash/zcash/blob/v1.0.15/doc/payment-api.md


Setup
-----

There are a couple options to setup Zcash for the first time.

	1. If you would like to compile Zcash from source, please skip to the Installation section.
	2. If you would like to install binary packages for Debian-based operating systems:
	   https://github.com/zcash/zcash/wiki/Debian-binary-packages

Installation
------------

Before we begin installing Zcash, we need to get some dependencies for your system.

    - Ubuntu/Debian-based System:
	  .. code-block:: bash
	     
	     sudo apt-get install
	     build-essential pkg-config libc6-dev m4 g++-multilib
	     autoconf libtool ncurses-dev unzip git python python-zmq
	     zlib1g-dev wget curl bsdmainutils automake

    - Fedora-based System:
	  .. code-block:: bash

	     sudo dnf install
	     git pkgconfig automake autoconf ncurses-devel python
	     python-zmq wget curl gtest-devel gcc gcc-c++ libtool patch

    - RHEL-based Systems(including Scientific Linux):
    	- Install devtoolset-3 and autotools-latest (if not previously installed).
    	- Run ``scl enable devtoolset-3 'scl enable autotools-latest bash'`` and do the remainder of the build in the shell that this starts.

Next, we need to ensure we have the correct version of `gcc` and `binutils`

	1. gcc/g++ 4.9 *or later* is required. 
	   Zcash has been successfully built using gcc/g++ versions 4.9 to 7.x inclusive. 

	   Use ``g++ --version`` to check which version you have.

	   On Ubuntu Trusty, if your version is too old then you can install gcc/g++ 4.9 as follows:

	   .. code-block:: bash

   		  $ sudo add-apt-repository ppa:ubuntu-toolchain-r/test
   	 	  $ sudo apt-get update
   		  $ sudo apt-get install g++-4.9

   	2. binutils 2.22 *or later* is required. 

	   Use ``as --version`` to check which version you have, and upgrade if necessary.

Now we need to get the Zcash software from the repository:

.. code-block:: bash

   git clone https://github.com/zcash/zcash.git
   cd zcash/
   git checkout v1.0.15
   ./zcutil/fetch-params.sh

This will fetch our Sprout proving and verifying keys (the final ones created in the [Parameter Generation Ceremony](https://github.com/zcash/mpc)), and place them into `~/.zcash-params/`. These keys are just under 911MB in size, so it may take some time to download them.

The message printed by ``git checkout`` about a "detached head" is normal and does not indicate a problem.

Build
-----

Ensure you have successfully installed all system package dependencies as described above. Then run the build, e.g.:

.. code-block:: bash
   
   ./zcutil/build.sh -j$(nproc)

.. note:: This should compile our dependencies and build `zcashd`. (Note: if you don't have `nproc`, then substitute the number of cores on your system. If the build runs out of memory, try again without the `-j` argument, i.e. just `./zcutil/build.sh`.

Configuration
-------------

Following the steps below will create your zcashd configuration file which can be edited to either connect to `mainnet` or `testnet` as well as applying settings to safely access the RPC interface.

Mainnet
*******

Create the `~/.zcash` directory and place a configuration file at `~/.zcash/zcash.conf` using the following commands:

.. code-block:: bash
   
   mkdir -p ~/.zcash
   echo "addnode=mainnet.z.cash" >~/.zcash/zcash.conf

.. note:: Note that this will overwrite any `zcash.conf` settings you may have added from testnet. (If you want to run on testnet, you can retain a `zcash.conf` from testnet.)

Testnet
*******

After running the above commands to create the `zcash.conf` file, edit the following parameters in your `zcash.conf` file to indicate network and node discovery for `testnet`:

	- add the line `testnet=1`
	- `addnode=testnet.z.cash` instead of `addnode=mainnet.z.cash`

Usage
-----

Now, run zcashd!

.. code-block:: bash
   
   ./src/zcashd

To run it in the background (without the node metrics screen that is normally displayed) use ``./src/zcashd --daemon``.

You should be able to use the RPC after it finishes loading. Here's a quick way to test:

.. code-block:: bash
   
   ./src/zcash-cli getinfo

.. note:: If you are familiar with bitcoind's RPC interface, you can use many of those calls to send ZEC between `t-addr` addresses. We do not support the 'Accounts' feature (which has also been deprecated in ``bitcoind``) — only the empty string ``""`` can be used as an account name. The main network node at mainnet.z.cash is also accessible via Tor hidden service at zcmaintvsivr7pcn.onion.

Using Zcash
***********

First, you want to obtain Zcash. You can purchase them from an exchange, from other users, or sell goods and services for them! Exactly how to obtain Zcash (safely) is not in scope for this document, but you should be careful. Avoid scams!

Generating a t-addr
+++++++++++++++++++

Let's generate a t-addr first.

.. code-block:: bash

   $ ./src/zcash-cli getnewaddress
   t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1

Listing t-addr
++++++++++++++

.. code-block:: bash

   $ ./src/zcash-cli getaddressesbyaccount ""

This should show the address that was just created.

Receiving Zcash with a z-addr
+++++++++++++++++++++++++++++

Now let's generate a z-addr.

.. code-block:: bash
   
   $ ./src/zcash-cli z_getnewaddress
   zcBqWB8VDjVER7uLKb4oHp2v54v2a1jKd9o4FY7mdgQ3gDfG8MiZLvdQga8JK3t58yjXGjQHzMzkGUxSguSs6ZzqpgTNiZG

This creates a private address and stores its key in your local wallet file. Give this address to the sender!

A z-addr is pretty large, so it's easy to make mistakes with them. Let's put it in an environment variable to avoid mistakes:

.. code-block:: bash

   $ ZADDR='zcBqWB8VDjVER7uLKb4oHp2v54v2a1jKd9o4FY7mdgQ3gDfG8MiZLvdQga8JK3t58yjXGjQHzMzkGUxSguSs6ZzqpgTNiZG'

Listing z-addr
++++++++++++++

To get a list of all addresses in your wallet for which you have a spending key, run this command:

.. code-block:: bash

   $ ./src/zcash-cli z_listaddresses

You should see something like:

.. code-block:: json

   [
    {
        "txid" : "af1665b317abe538148114a45322f28151925501c081949cc7a5207ef21cb750",
        "amount" : 1.23,
        "memo" : "48656c6c6f20ceb2210000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    }
   ]

Sending coins with your z-addr
++++++++++++++++++++++++++++++

If someone gives you their z-addr...

.. code-block:: bash

   $ FRIEND='zcCDe8krwEt1ozWmGZhBDWrcUfmK3Ue5D5z1f6u2EZLLCjQq7mBRkaAPb45FUH4Tca91rF4R1vf983ukR71kHyXeED4quGV'

You can send 0.8 ZEC by doing...
.. code-block:: bash
   
   $ ./src/zcash-cli z_sendmany "$ZADDR" "[{\"amount\": 0.8, \"address\": \"$FRIEND\"}]"

After waiting about a minute, you can check to see if the operation has finished and produced a result:
.. code-block:: bash

   $ ./src/zcash-cli z_getoperationresult

.. code-block:: json

   [
    {
        "id" : "opid-4eafcaf3-b028-40e0-9c29-137da5612f63",
        "status" : "success",
        "creation_time" : 1473439760,
        "result" : {
            "txid" : "3b85cab48629713cc0caae99a49557d7b906c52a4ade97b944f57b81d9b0852d"
        },
        "execution_secs" : 51.64785629
    }
   ]


Additional operations for zcash-cli
+++++++++++++++++++++++++++++++++++

As Zcash is an extension of bitcoin, zcash-cli supports all commands that are part of the Bitcoin Core API (as of version 0.11.2), https://en.bitcoin.it/wiki/Original_Bitcoin_client/API_calls_list

For a full list of new commands that are not part of bitcoin API (mostly addressing operations on z-addrs) see https://github.com/zcash/zcash/blob/master/doc/payment-api.md

To list all zcash commands, use `./src/zcash-cli help`.

To get help with a particular command, use `./src/zcash-cli help <command>`.

.. attention:: 
   Known Security Issues

   Each release contains a `./doc/security-warnings.md` document describing
   security issues known to affect that release. You can find the most
   recent version of this document here:

   https://github.com/zcash/zcash/blob/master/doc/security-warnings.md

   Please also see our security page for recent notifications and other
   resources:

   https://z.cash/support/security.html
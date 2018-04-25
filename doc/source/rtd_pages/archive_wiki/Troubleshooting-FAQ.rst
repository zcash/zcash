The General FAQ has been reorganized and relocated to
https://z.cash/support/faq.html. Please check there for the latest
updates to our frequently asked questions. The following is a list of
questions for Troubleshooting zcashd, the core Zcash client software.

System Requirements
-------------------

-  64-bit Linux (easiest with a Debian-based distribution)
-  A compiler for C++11 if building from source. Gcc 6.x and above has
   full C++11 support, and gcc 4.8 and above supports some but not all
   features. Zcash will not compile with versions of gcc lower than 4.8.
-  At least 4GB of RAM to generate shielded transactions.
-  At least 8GB of RAM to successfully run all of of the tests.

Zcash runs on port numbers that are 100 less than the corresponding
Bitcoin port number. They are:

-  8232 for mainnet RPC
-  8233 for mainnet peer-to-peer network
-  18232 for testnet RPC
-  18233 for testnet peer-to-peer network

Building from source
--------------------

If you did not build by running ``build.sh``, you will encounter errors.
Be sure to build with:

::

    $ ./zcutil/build.sh -j$(nproc)

(Note: If you don't have nproc, then substitute the number of your
processors.)

Error message: ``g++: internal compiler error: Killed (program cc1plus)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This means your system does not have enough memory for the building
process and has failed. Please allocate at least 4GB of computer memory
for this process and try again.

Error message: ``'runtime_error'`` (or other variable) ``is not a member of 'std'. compilation terminated due to -Wfatal-errors.``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Check your compiler version and ensure that it support C++11. If you're
using a version of gcc below 4.8.x, you will need to upgrade.

Error message: gtest failing with undefined reference
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you are developing on different branches of Zcash, there may be an
issue with different versions of linked libraries. Try ``make clean``
and build again.

Running Zcashd
--------------

Trying to start Zcashd for the first time, it fails with ``could not load param file at /home/rebroad/.zcash-params/sprout-verifying.key``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You didn't fetch the parameters necessary for zk-SNARK proofs. If you
installed the Debian package, run ``zcash-fetch-params``. If you built
from source, run ``./zcutil/fetch-params.sh``.

Zcashd crashes with the message ``std::bad_alloc`` or ``St13runtime_exception``.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These messages indicate that your computer has run out of memory for
running zcashd. This will most likely happen with mining nodes which
require more resources than a full node without running a miner. This
can also happen while creating a transaction involving a z-address.
You'll need to allocate at least 4GB memory for these transactions.

RPC Interface
-------------

To get help with the RPC interface from the command line, use
``zcash-cli help`` to list all commands.

To get help with a particular command, use ``zcash-cli help $COMMAND``.

There is also additional documentation under
`doc/payment-api.md <https://github.com/zcash/zcash/blob/v1.0.4/doc/payment-api.md>`__.

``zcash-cli`` stops responding after I use the command ``z_importkey``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The command has added the key, but your node is currently scanning the
blockchain for any transactions related to that key, causing there to be
a delay before it returns. This immediate rescan is the default setting
for ``z_importkey``, which you can override by adding ``false`` to the
command if you simply want to import the key, i.e.
``zcash-cli z_importkey $KEY false``

What if my question isn't answered here?
----------------------------------------

First search the issues section (https://github.com/zcash/zcash/issues)
to see if someone else has posted a similar issue and if not, feel free
to report your problem there. Please provide as much information about
what you've tried and what failed so others can properly assess your
situation to help.

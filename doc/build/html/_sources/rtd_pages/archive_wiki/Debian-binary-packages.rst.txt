Zcash Company operates a package repository for 64-bit Debian-based
distributions. If you'd like to try out the binary packages, you can set
it up on your system and install Zcash from there.

First install the following dependency so you can talk to our repository
using HTTPS:

::

    sudo apt-get install apt-transport-https

Next add the Zcash master signing key to apt's trusted keyring:

::

    wget -qO - https://apt.z.cash/zcash.asc | sudo apt-key add -

Fingerprint: F1E2 1403 7E94 E950 BA85 77B2 63C4 A216 9C1B 2FA2

Add the repository to your sources:

::

    echo "deb [arch=amd64] https://apt.z.cash/ jessie main" | sudo tee /etc/apt/sources.list.d/zcash.list

Finally, update the cache of sources and install Zcash:

::

    sudo apt-get update && sudo apt-get install zcash

Lastly you can run ``zcash-fetch-params`` to fetch the zero-knowledge
parameters, and set up ``~/.zcash/zcash.conf`` before running Zcash as
your local user, as documented in the `User
Guide <https://github.com/zcash/zcash/wiki/1.0-User-Guide>`__.

Troubleshooting
~~~~~~~~~~~~~~~

**Note:** Only x86-64 processors are supported.

If you're starting from a new virtual machine, sudo may not come
installed. See this issue for instructions to get up and running:
https://github.com/zcash/zcash/issues/1844

libgomp1 or libstdc++6 version problems
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These libraries are provided with gcc/g++. If you see errors related to
updating them, you may need to upgrade your gcc/g++ packages to version
4.9 or later. First check which version you have using
``g++ --version``; if it is before 4.9 then you will need to upgrade.

On Ubuntu Trusty, you can install gcc/g++ 4.9 as follows:

::

    sudo add-apt-repository ppa:ubuntu-toolchain-r/test
    sudo apt-get update
    sudo apt-get install g++-4.9

Tor
~~~

The repository is also accessible via Tor, after installing the
``apt-transport-tor`` package, at the address zcaptnv5ljsxpnjt.onion.
Use the following pattern in your sources.list file:
``deb [arch=amd64] tor+http://zcaptnv5ljsxpnjt.onion/ jessie main``

VoteCoin 1.0.12

What is VoteCoin?
-----------------

[VoteCoin](https://votecoin.site/) is an implementation of the "Zerocash" protocol.
Forked from ZCash, based on Bitcoin's code, it intends to offer a far higher standard of privacy
through a sophisticated zero-knowledge proving scheme that preserves
confidentiality of transaction metadata. Technical details are available
in our [Protocol Specification](https://github.com/zcash/zips/raw/master/protocol/protocol.pdf).

This software is the VoteCoin client. It downloads and stores the entire history
of VoteCoin transactions; depending on the speed of your computer and network
connection, the synchronization process could take a day or more once the
blockchain has reached a significant size.

Security Warnings
-----------------

**VoteCoin is experimental and a work-in-progress.** Use at your own risk.


Windows release
---------------

There is a precompiled Windows version of VoteCoin. You can download it from
https://github.com/Tomas-M/VoteCoin/releases/
The download is 853 MB due to zkSNARK proving key which is included in the release.
Simply unzip and read README.txt. If you use precompiled Windows binaries, you can
skip to the section "Running" in this document now.


Building
--------

If you prefer to build VoteCoin from source code, you can do so by running
the following commands in Linux:

    git clone https://github.com/Tomas-M/VoteCoin.git
    cd ./VoteCoin/zcutil
    ./votecoin_build_debian.sh # for debian/ubuntu based systems

This will also setup your votecoin.conf file in ~/.votecoin directory, if the file does not exist yet.


Configuring
-----------

The above mentioned command should configure your votecoin already by creating ~/.votecoin/votecoin.conf for you. The file can be generated manually by running

    mkdir -p ~/.votecoin
    echo "addnode=mainnet.votecoin.site" >~/.votecoin/votecoin.conf
    echo "rpcuser=username" >>~/.votecoin/votecoin.conf
    echo "rpcpassword=`head -c 32 /dev/urandom | base64`" >>~/.votecoin/votecoin.conf


Mining
------

Mining with CPU is currently worthless. If you plan to mine VoteCoin, you should
join a pool. There are several pools in operation at the moment, for example:

    http://pool.votecoin.site ... the official mining pool from VoteCoin developers, 0% fee
    http://votecoinmine.site ... unofficial pool, 2% fee


Installing
----------

Compiled binaries can be found in ./src directory. Copy them to your working path:

    cp ./src/votecoin-cli /usr/bin
    cp ./src/votecoind /usr/bin
    cp ./zcutil/vot /usr/bin


Running
-------

    $ votecoind


Generating Transparent address
------------------------------

Once votecoind daemon is running, you may use votecoin-cli from another console to send commands to the daemon. For example, to generate new transparent address,
simply call:

    $ votecoin-cli getnewaddress
    t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1


Generating Shielded address
---------------------------

    $ votecoin-cli z_getnewaddress
    zcBqWB8VDjVER7uLKb4oHp2v54v2a1jKd9o4FY7mdgQ3gDfG8MiZLvdQga8JK3t58yjXGjQHzMzkGUxSguSs6ZzqpgTNiZG


Show balances
-------------

    $ votecoin-cli getbalance
    $ votecoin-cli z_getbalance t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1
    $ votecoin-cli z_getbalance zcBqWB8VDjVER7uLKb4oHp2v54v2a1jKd9o4FY7mdgQ3gDfG8MiZLvdQga8JK3t58yjXGjQHzMzkGUxSguSs6ZzqpgTNiZG


Make transaction
----------------

    $ votecoin-cli z_sendmany FROM_ADDR '[{"address": "TO_ADDR", "amount": 123}]' 1 0

In the above mentioned example, both FROM_ADDR and TO_ADDR can be either T addresses or Z addresses or combination.
The last two numbers (1 0) stand for minConf and fee. In the example above, the coins on FROM_ADDR must have
at least 1 confirmation, and fee is set to zero.

Alternatively, if you don't care from which address the coins are sent, you can use:

    $ votecoin-cli sendtoaddress TO_ADDR 123

In that case, coins may be combined from multiple addresses.

Remember that coinbase transaction (sending newly mined coins) must be done from a T address to a Z address.
So if you wish to send newly mined coins to a T address, you need to send them to a Z address first and then
make another transaction from that Z address to the T address destination.

After a transaction is executed using z_sendmany, you can check its status by using:

    $ votecoin-cli z_getoperationresult



Export wallet to a file (dump private keys)
-------------------------------------------

    $ votecoin-cli z_exportwallet exportfilename

You may need to specify export directory path using for example "exportdir=/tmp" in your ~/.votecoin/votecoin.conf


Shortcut
--------

There is a script called 'vot' for easier operation. It calls votecoin-cli with proper syntax. You may use it instead of votecoin-cli for selected commands:

    $ vot addr ... generate new T address
    $ vot zaddr ... generate new Z address
    $ vot send FROM TO AMOUNT ... send coins FROM address to TO address of given AMOUNT, with zero fee
    $ vot status ... show status of last transaction. Empty status means transaction still in progress
    $ vot totals ... show total balances in your entire wallet
    $ vot list ... list all addresses and their balances (non-zero only)
    $ vot export ... show all your wallet addresses including their private keys

Remember that the last two commands need to export your wallet to a temporary file, so if you are using a computer in shared environment (eg. a server where
other users can login) then you need to make sure other users do not have access to the exported file. The exported file is deleted after the vot script
finishes, but is accessible while the script is still running (eg. fetching balances etc).


License
-------

For license information see the file [COPYING](COPYING).

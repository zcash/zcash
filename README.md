VoteCoin 1.0.11
===============

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


Building
--------

Build VoteCoin along with most dependencies from source by running

    git clone https://github.com/Tomas-M/VoteCoin.git
    cd ./VoteCoin/zcutil
    ./votecoin_build_debian.sh # for debian based systems

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

    http://pool.votecoin.site
    http://votecoinmine.site


Running
-------

    $ ./src/votecoind


Generating Transparent address
------------------------------

    $ ./src/votecoin-cli getnewaddress
    t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1


Generating Shielded address
---------------------------

    $ ./src/votecoin-cli z_getnewaddress
    zcBqWB8VDjVER7uLKb4oHp2v54v2a1jKd9o4FY7mdgQ3gDfG8MiZLvdQga8JK3t58yjXGjQHzMzkGUxSguSs6ZzqpgTNiZG


Show balances
-------------

    $ ./src/votecoin-cli getbalance
    $ ./src/votecoin-cli z_getbalance t14oHp2v54vfmdgQ3v3SNuQga8JKHTNi2a1
    $ ./src/votecoin-cli z_getbalance zcBqWB8VDjVER7uLKb4oHp2v54v2a1jKd9o4FY7mdgQ3gDfG8MiZLvdQga8JK3t58yjXGjQHzMzkGUxSguSs6ZzqpgTNiZG


Make transaction
----------------

    $ ./src/votecoin-cli z_sendmany FROM_ADDR '[{"address": "TO_ADDR", "amount": 123}]' 1 0

In the above mentioned example, both FROM_ADDR and TO_ADDR can be either T addresses or Z addresses or combination.
The last two numbers (1 0) stand for minConf and fee. In the example above, the coins on FROM_ADDR must have
at least 1 confirmation, and fee is set to zero.

Remember that coinbase transaction (sending newly mined coins) must be done from a T address to a Z address.
So if you wish to send newly mined coins to a T address, you need to send them to a Z address first and then
make another transaction from that Z address to the T address destination.

After a transaction is executed using z_sendmany, you can check its status by using:

    $ ./src/votecoin-cli z_getoperationresult



Export wallet to a file (dump private keys)
-------------------------------------------

    $ ./src/votecoin-cli z_exportwallet exportfilename

You may need to specify export directory path using for example "exportdir=/tmp" in your ~/.votecoin/votecoin.conf


License
-------

For license information see the file [COPYING](COPYING).

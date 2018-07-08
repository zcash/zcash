## Komodo with Bitcore
This version of Komodo contains Bitcore support for komodo and all its assetchains.

## Komodod
This software is Komodo client, generally you will use this if you want to mine KMD or setup a full node.
It downloads and stores the entire history of Komodo transactions; depending on the speed of your computer and network connection, the synchronization process could take a day or more once the blockchain has reached a significant size.

## Development Resources
- Komodo Web: [https://komodoplatform.com/](https://komodoplatform.com/)
- Organization web: [https://komodoplatform.com/](https://komodoplatform.com/)
- Forum: [https://forum.komodoplatform.com/](https://forum.komodoplatform.com/)
- Mail: [info@komodoplatform.com](mailto:info@komodoplatform.com)
- Support: [https://support.komodoplatform.com/support/home](https://support.komodoplatform.com/support/home)
- Knowledgebase & How-to: [https://komodoplatform.atlassian.net/wiki/spaces/KPSD/pages](https://komodoplatform.atlassian.net/wiki/spaces/KPSD/pages)
- API references: [http://docs.supernet.org/](http://docs.supernet.org/) #Not up to date.
- Whitepaper: [Komodo Whitepaper](https://komodoplatform.com/wp-content/uploads/2018/06/Komodo-Whitepaper-June-3.pdf)
- Komodo Platform public material: [Komodo Platform public material](https://docs.google.com/document/d/1AbhWrtagu4vYdkl-vsWz-HSNyNvK-W-ZasHCqe7CZy0)

## List of Komodo Platform Technologies
- Delayed Proof of Work (dPoW) - Additional security layer.
- zk-SNARKs - Komodo Platform's privacy technology
- Jumblr - Decentralized tumbler for KMD and other cryptocurrencies
- Assetchains - Easy way to fork Komodo coin
- Pegged Assets - Chains that maintain a peg to fiat currencies
- Peerchains - Scalability solution where sibling chains form a network of blockchains
- More in depth covered [here](https://docs.google.com/document/d/1AbhWrtagu4vYdkl-vsWz-HSNyNvK-W-ZasHCqe7CZy0)
- Also note you receive 5% APR on your holdings.
[See this article for more details](https://komodoplatform.atlassian.net/wiki/spaces/KPSD/pages/20480015/Claim+KMD+Interest+in+Agama)

## Tech Specification
- Max Supply: 200 million KMD.
- Block Time: 1M 2s
- Block Reward: 3KMD
- Mining Algorithm: Equihash 

## About this Project
Komodo is based on Zcash and has been  by our innovative consensus algorithm called dPoW which utilizes Bitcoin's hashrate to store Komodo blockchain information into the Bitcoin blockchain. Other new and native Komodo features are the privacy technology called JUMBLR or our assetchain capabilities (one click plug and play blockchain solutions). More details are available under https://komodoplatform.com/. 

## Getting started
Dependencies
------------

```shell
#The following packages are needed:
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python python-zmq zlib1g-dev wget libcurl4-openssl-dev bsdmainutils automake curl
```

Komodo
------
We have a release process that goes through several stages before it reaches master. This allows the most conservative users just use the master branch, which is only updated after the other branches have signed off on a release.

99% of the activity is in the dev branch, this is where I am testing each change one by one and there are literally thousands of updates. Only use this branch if you really want to be on the bleeding edge. I try to keep things stable, but there are times where necessarily there are bugs in the dev branch, since I am actively developing and debugging here. A good rule is to wait for at least 4 hours from the last update before using the dev branch (unless you know what you are doing)

After things look good in the dev branch, it is propagated to the beta branch, this is the version the notary nodes use. They are knowledegable command line server guys and so they have a keen eye for anything that wasnt caught during the dev cycle.

After the notary nodes verify things are working and the latest release is deemed stable, it is propagated to the dPoW branch. From here an automated Jenkins process builds it for all OS, and since the notary nodes are all unix, it is possible for some issues to be caught at this stage. The dPoW branch is what goes into the GUI installers.

After the GUI are updated and released and it is verified that no significant support issues were created, the master branch is finally updated.

Master branch: exchanges and users that build from the repo without changing branches
dPoW branch: autobuild into GUI installers, unix, osx, windows
beta branch: notary nodes, command line unix
dev branch: bleeding edge, possibly wont even compile, multiple updates per hour

```shell
git clone https://github.com/jl777/komodo
cd komodo
#you might want to: git checkout <branch>; git pull
./zcutil/fetch-params.sh
# -j8 uses 8 threads - replace 8 with number of threads you want to use
./zcutil/build.sh -j8
#This can take some time.
```

**komodo is experimental and a work-in-progress.** Use at your own risk.

Deprecation Policy
------------------

This release is considered deprecated one year after the release day. There
is an automatic deprecation shutdown feature which will halt the node some
time after this one year period. The automatic feature is based on block
height and can be explicitly disabled.


# to update an existing version, `git checkout dPoW` if not on that branch already
```shell
git pull
./zcutil/fetch-params.sh
./zcutil/build.sh -j8
```
To reset the blockchain, from *~/.komodo* `rm -rf blocks chainstate debug.log komodostate db.log`

Create komodo.conf
------------------

```
cd ~
mkdir .komodo
cd .komodo
pico komodo.conf
#Add the following lines to the komodo.conf file:
rpcuser=bitcoinrpc
rpcpassword=password
txindex=1
addnode=5.9.102.210
addnode=78.47.196.146
addnode=178.63.69.164
addnode=88.198.65.74
addnode=5.9.122.241
addnode=144.76.94.38
addnode=89.248.166.91
```

Start mining
------------

```shell
#iguana documentation shows how to get the btcpubkey and wifstrs that need to be used
#bitcoin also need to be installed with txindex=1 and with rpc enabled
cd ~
cd komodo
#This will return your pubkey eg. "0259e137e5594cf8287195d13aed816af75bd5c04ae673296b51f66e7e8346e8d8" for your address
./src/komodo-cli validateaddress <yourwalletaddres>
#This will give the privkey of your wallet address
./src/komodo-cli dumpprivkey <yourwalletaddres>
#This will import the privkey to be sure the mined coins are placed into your wallet address
./src/komodo-cli importprivkey <yourwalletprivkey>
#To stop the daemon:
./src/komodo-cli stop
#This starts komodo notary - replace genproclimit with number of threads you want to use and add your pubkey
./src/komodod -gen -genproclimit=2 -notary -pubkey="0259e137e5594cf8287195d13aed816af75bd5c04ae673296b51f66e7e8346e8d8" &
#This will get the stats:
./src/komodo-cli getinfo
#To view the process:
ps -ef | grep komodod
#To stop the daemon:
./src/komodo-cli stop 

#To view komodod output:
tail -f ~/.komodo/debug.log
#To view all command
./src/komodo-cli help
ASSETCHAINS: -ac_name=name -ac_supply=nnnnn
Both komodod and komodo-cli recognize -ac_name=option so you can create fork from the commandline
```
=======
**Zcash is unfinished and highly experimental.** Use at your own risk.

Where do I begin?
-----------------
We have a guide for joining the main Zcash network:
https://github.com/zcash/zcash/wiki/1.0-User-Guide

### Need Help?
* See the documentation at the [Zcash Wiki](https://github.com/zcash/zcash/wiki)
for help and more information.
* Ask for help on the [Zcash](https://forum.z.cash/) forum.
Participation in the Zcash project is subject to a
[Code of Conduct](code_of_conduct.md).

Building
--------
Build Zcash along with most dependencies from source by running
`./zcutil/build.sh`. Currently only Linux is officially supported.

License
-------
For license information see the file [COPYING](COPYING).

**NOTE TO EXCHANGES:**
https://bitcointalk.org/index.php?topic=1605144.msg17732151#msg17732151
There is a small chance that an outbound transaction will give an error due to mismatched values in wallet calculations. There is a -exchange option that you can run komodod with, but make sure to have the entire transaction history under the same -exchange mode. Otherwise you will get wallet conflicts.

**To change modes:**

a) backup all privkeys (launch komodod with `-exportdir=<path>` and `dumpwallet`)
  
b) start a totally new sync including `wallet.dat`, launch with same `exportdir`

c) stop it before it gets too far and import all the privkeys from a) using `komodo-cli importwallet filename`

d) resume sync till it gets to chaintip

For example:
```shell
./komodod -exportdir=/tmp &
./komodo-cli dumpwallet example
./komodo-cli stop
mv ~/.komodo ~/.komodo.old && mkdir ~/.komodo && cp ~/.komodo.old/komodo.conf ~/.komodo.old/peers.dat ~/.komodo
./komodod -exchange -exportdir=/tmp &
./komodo-cli importwallet /tmp/example
```

## JUMBLR
komodod now has `jumblr_deposit` and `jumblr_secret` RPC calls.

Jumblr works like described previously where all the nodes with jumblr active synchronize their tx activity during the same block to maximize the mixing effect. However, unlike all other mixers/tumblers, you never give up control of your coins to anybody else. JUMBLR uses a one to many allocation of funds, ie. one deposit address and many secret addresses. You can always run multiple komodod daemons to get multiple active deposit addresses.

JUMBLR implements t -> z, z -> z and z -> t transactions to maximize privacy of the destination t (transparent) address. So while it is transparent, its first activity is funds coming from an untracable z address.

Which of the three stages is done is randomly selected at each turn. Also when there is more than one possible transaction at the selected stage, a random one is selected. This randomization prevents analyzing incoming z ->t transactions by its size to correlate it to the originating address.

`jumblr_deposit <depositaddr>` designates the deposit address as the jumblr deposit address for that session. You can select an address that already has funds in it and it will immediately start jumblr process. If there are no funds, it will wait until you send funds to it.
  
There are three sizes of a jumblr transaction: 10 KMD, 100 KMD and 1000 KMD. There is also a fixed interval of blocks where all jumblr nodes are active. Currently it is set to be 10, but this is subject to change. Only during every 10*10 blocks are the largest 1000 KMD transactions processed, so this concentrates all the large transactions every N*N blocks.

`jumblr_secret <secretaddress>` notifies JUMBLR where to send the final z -> t transactions. In order to allow larger accounts to obtain privacy, up to 777 secret addresses are supported. Whenever a z -> t stage is activated, a random secret address from the list of the then active secret addresses is selected.

#### Practical Advice:
Obtaining privacy used to be very difficult. JUMBLR makes it as simple as issuing two command line calls. Higher level layers can be added to help manage the addresses, ie. linking them at the passphrase level. Such matters are left to each implementation.

Once obtained, it is very easy to lose all the privacy. With a single errant transaction that combines some previously used address and the secretaddress, well, the secretaddress is no longer so private.

The advice is to setup a totally separate node!

This might seem a bit drastic, but if you want to maintain privacy, it is best to make it look like all the transactions are coming from a different node. The easiest way for most people to do this is to actually have a different node.

It can be a dedicated laptop (recommended) or a VPS (for smaller amounts) with a totally fresh komodod wallet. Generate an address on this wallet and use that as the jumblr_secret address on your main node. As the JUMBLR operates funds will teleport into your secret node's address. If you are careful and never use the same IP address for both your nodes, you will be able to maintain very good privacy.

Of course, don't send emails that link the two accounts together! Dont use secret address funds for home delivery purchases! Etc. There are many ways to lose the privacy, just think about what linkages can be dont at the IP and blockchain level and that should be a useful preparation.

What if you have 100,000 KMD and you dont want others to know you are such a whale?
Instead of generating 1 secret address, generate 100 and make a script file with:
```shell
./komodo-cli jumblr_secret <addr0>
./komodo-cli jumblr_secret <addr1>
...
./komodo-cli jumblr_secret <addr99>
```
And make sure to delete all traces of this when the JUMBLR is finished. You will end up with 100 addresses that have an average of 1000 KMD each. So as long as you are careful and dont do a 10,000 KMD transaction (that will link 10 of your secret addresses together), you can appear as 100 different people each with 1000 KMD.




## VerusCoin version 0.3.12-beta
VerusCoin is a new, mineable and stakeable cryptocurrency. It is a live fork of Komodo that retains its Zcash lineage and improves it. VerusCoin will leverage the Komodo platform and dPoW notarization for enhanced security and cross-chain interoperability. We have added a variation of a zawy12, lwma difficulty algorithm, a new CPU-optimized hash algorithm and a new algorithm for fair proof of stake. We describe these changes and vision going forward in a [our Phase I white paper](http://185.25.51.16/papers/VerusPhaseI.pdf) and [our Vision](http://185.25.51.16/papers/VerusVision.pdf).
- [VerusCoin web site https://veruscoin.io/ Wallets and CLI tools](https://veruscoin.io/)
- [VerusCoin Explorer](https://explorer.veruscoin.io/)

Version 0.3.12-beta has portable mining working.

## Komodo with Bitcore
This version of Komodo contains Bitcore support for komodo and all its assetchains.

## Komodod
This software is the VerusCoin enhanced Komodo client. Generally, you will use this if you want to mine VRSC or setup a full node. When you run the wallet it launches komodod automatically. On first launch it downloads Zcash parameters, roughly 1GB, which is mildly slow.
The wallet downloads and stores the block chain or asset chain of the coin you select. It downloads and stores the entire history of the coins transactions; depending on the speed of your computer and network connection, the synchronization process could take a day or more once the blockchain has reached a significant size.

## Development Resources
- VerusCoin:[https://veruscoin.io/](https://veruscoin.io/) Wallets and CLI tools
- Komodo Web: [https://komodoplatform.com/](https://komodoplatform.com/)
- Organization web: [https://komodoplatform.com/](https://komodoplatform.com/)
- Forum: [https://forum.komodoplatform.com/](https://forum.komodoplatform.com/)
- Mail: [info@komodoplatform.com](mailto:info@komodoplatform.com)
- Support: [https://support.komodoplatform.com/support/home](https://support.komodoplatform.com/support/home)
- Knowledgebase & How-to: [https://komodoplatform.atlassian.net/wiki/spaces/KPSD/pages](https://komodoplatform.atlassian.net/wiki/spaces/KPSD/pages)
- API references: [http://docs.supernet.org/](http://docs.supernet.org/) #Not up to date.
- Whitepaper: [Komodo Whitepaper](https://komodoplatform.com/wp-content/uploads/2018/03/2018-03-12-Komodo-White-Paper-Full.pdf)
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


Building
--------

First time you'll need to get assorted startup values downloaded. This takes a moderate amount of time once but then does not need to be repeated unless you bring a new system up. The command is:
```
./zcutil/fetch-params.sh
```
Building for Ubunutu/Mint:
```
./zcutil/build.sh
```
Building for Mac OS/X (see README-MAC.md):
```
./zcutil/build-mac.sh
```
Building for Windows:
```
./zcutil/build-win.sh
```
VerusCoin
------
We develop on dev and some other branches and produce releases of of the master branch, using pull requests to manage what goes into master.


```shell
git clone https://github.com/VerusCoin/VerusCoin
cd VerusCoin
#you might want to: git checkout <branch>; git pull
./zcutil/fetch-params.sh
# -j8 uses 8 threads - replace 8 with number of threads you want to use
./zcutil/build.sh -j8
#This can take some time.
```

**The VerusCoin enhanced komodo is experimental and a work-in-progress.** Use at your own risk.


#To view komodod output:
tail -f ~/.komodo/debug.log
#To view VRSC output:
tail -f ~/.komodo/VRSC/debug.log
Note that this directory is correct for Linux, not Mac or Windows
#To view all command
./src/komodo-cli help
**Zcash is unfinished and highly experimental.** Use at your own risk.

Where do I begin?
-----------------
We have a guide for joining the main Zcash network:
https://github.com/zcash/zcash/wiki/1.0-User-Guide

#Older Komodo Details
The remaining text is from the komodo source we forked when creating VerusCoin/Veruscoin.
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

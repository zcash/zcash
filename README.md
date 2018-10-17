## VerusCoin version 0.4.1

VerusCoin is a new, mineable and stakeable cryptocurrency. It is a live fork of Komodo that retains its Zcash lineage and improves it. VerusCoin will leverage the Komodo platform and dPoW notarization for enhanced security and cross-chain interoperability. We have added a variation of a zawy12, lwma difficulty algorithm, a new CPU-optimized hash algorithm and a new algorithm for fair proof of stake. We describe these changes and vision going forward in a [our Phase I white paper](http://185.25.51.16/papers/VerusPhaseI.pdf) and [our Vision](http://185.25.51.16/papers/VerusVision.pdf).
- [VerusCoin web site https://veruscoin.io/ Wallets and CLI tools](https://veruscoin.io/)
- [VerusCoin Explorer](https://explorer.veruscoin.io/)


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
- API references: [http://docs.komodoplatform.com/](http://docs.komodoplatform.com/)
- Blog: [http://blog.komodoplatform.com/](http://blog.komodoplatform.com/)
- Whitepaper: [Komodo Whitepaper](https://komodoplatform.com/wp-content/uploads/2018/03/2018-03-12-Komodo-White-Paper-Full.pdf)
- Komodo Platform public material: [Komodo Platform public material](https://docs.google.com/document/d/1AbhWrtagu4vYdkl-vsWz-HSNyNvK-W-ZasHCqe7CZy0)

## List of Komodo Platform Technologies

- Delayed Proof of Work (dPoW) - Additional security layer and Komodos own consensus algorithm.
- zk-SNARKs - Komodo Platform's privacy technology for shielded transactions
- Tokens/Assets Technology - create "colored coins" on the Komodo Platform and use them as a layer for securites
- Reward API - Komodo CC technology for securities
- CC - Crypto Conditions to realize "smart contract" logic on top of the Komodo Platform
- Jumblr - Decentralized tumbler for KMD and other cryptocurrencies
- Assetchains - Create your own Blockchain that inherits all Komodo Platform functionalities and blockchain interoperability
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
Komodo is based on Zcash and has been extended by our innovative consensus algorithm called dPoW which utilizes Bitcoin's hashrate to store Komodo blockchain information into the Bitcoin blockchain. Other new and native Komodo features are the privacy technology called JUMBLR, our assetchain capabilities (one click plug and play blockchain solutions) and a set of financial decentralization and interoperability technologies. More details are available under https://komodoplatform.com/ and https://blog.komodoplatform.com.

## Getting started

### Dependencies

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
We develop on dev and some other branches and produce releases of of the master branch, using pull requests to manage what goes into master. The dev branch is considered the bleeding edge codebase, and may even be oncompatible from time to time, while the master-branch is considered tested (unit tests, runtime tests, functionality). At no point of time do the Komodo Platform developers or Verus Developers take any responsbility for any damage out of the usage of this software. 

Verus builds for all operating systems out of the same codebase. Follow the OS specific instructions from below.

#### Linux
```shell
git clone https://github.com/VerusCoin/VerusCoin
cd VerusCoin
#you might want to: git checkout <branch>; git pull
./zcutil/fetch-params.sh
# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use
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

####  :ledger: Deprecation Policy

This release is considered deprecated 16 weeks after the release day. There
is an automatic deprecation shutdown feature which will halt the node some
time after this 16 week time period. The automatic feature is based on block
height.

#Older Komodo Details
The remaining text is from the komodo source we forked when creating VerusCoin/Veruscoin.

**To change modes:**

a) backup all privkeys (launch komodod with `-exportdir=<path>` and `dumpwallet`)
b) start a totally new sync including `wallet.dat`, launch with same `exportdir`
c) stop it before it gets too far and import all the privkeys from a) using `komodo-cli importwallet filename`
d) resume sync till it gets to chaintip

For example:
```shell
./verusd -exportdir=/tmp &
./verus dumpwallet example
./verus stop
mv ~/.komodo/VRSC ~/.komodo/VRSC.old && mkdir ~/.komodo/VRSC && cp ~/.komodo/VRSC.old/VRSC.conf ~/.komodo/VRSC.old/peers.dat ~/.komodo/VRSC
./verusd -exchange -exportdir=/tmp &
./verus importwallet /tmp/example
```
---


Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notices and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

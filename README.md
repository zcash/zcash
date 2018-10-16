[![Build Status](https://travis-ci.org/KomodoPlatform/komodo.svg?branch=dev)](https://travis-ci.org/KomodoPlatform/komodo)
---
![Komodo Logo](https://i.imgur.com/vIwVtqv.png "Komodo Logo")


## Komodo

This is the official Komodo sourcecode repository based on https://github.com/jl777/komodo. 

## Development Resources

- Komodo Website: [https://komodoplatform.com](https://komodoplatform.com/)
- Komodo Blockexplorer: [https://kmdexplorer.io](https://kmdexplorer.io/)
- Komodo Discord: [https://komodoplatform.com/discord](https://komodoplatform.com/discord)
- Forum: [https://forum.komodoplatform.com](https://forum.komodoplatform.com/)
- Mail: [info@komodoplatform.com](mailto:info@komodoplatform.com)
- Support: [https://support.komodoplatform.com/support/home](https://support.komodoplatform.com/support/home)
- Knowledgebase & How-to: [https://support.komodoplatform.com/en/support/solutions](https://support.komodoplatform.com/en/support/solutions)
- API references & Dev Documentation: [https://docs.komodoplatform.com](https://docs.komodoplatform.com/)
- Blog: [https://blog.komodoplatform.com](https://blog.komodoplatform.com/)
- Whitepaper: [Komodo Whitepaper](https://komodoplatform.com/whitepaper)
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

### Build Komodo

This software is based on zcash and considered experimental and is continously undergoing heavy development.

The dev branch is considered the bleeding edge codebase while the master-branch is considered tested (unit tests, runtime tests, functionality). At no point of time do the Komodo Platform developers take any responsbility for any damage out of the usage of this software. 
Komodo builds for all operating systems out of the same codebase. Follow the OS specific instructions from below.

#### Linux
```shell
git clone https://github.com/komodoplatform/komodo --branch master --single-branch
cd komodo
./zcutil/fetch-params.sh
# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use
./zcutil/build.sh -j8
#This can take some time.
```

#### OSX
Ensure you have [brew](https://brew.sh) and the command line tools installed (comes automatically with XCode) and run:
```shell
brew update && brew install gcc@6
git clone https://github.com/komodoplatform/komodo --branch master --single-branch
cd komodo
./zcutil/fetch-params.sh
# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use
./zcutil/build-mac.sh -j8
#This can take some time.
```

#### Windows
Use a debian cross-compilation setup with mingw for windows and run:
```shell
git clone https://github.com/komodoplatform/komodo --branch master --single-branch
cd komodo
./zcutil/fetch-params.sh
# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use
./zcutil/build-win.sh -j8
#This can take some time.
```
**komodo is experimental and a work-in-progress.** Use at your own risk.

To reset the Komodo blockchain change into the *~/.komodo* data directory and delete the corresponding files by running `rm -rf blocks chainstate debug.log komodostate db.log`

#### Create komodo.conf

Create a komodo.conf file:

```
mkdir ~/.komodo
cd ~/.komodo
touch komodo.conf

#Add the following lines to the komodo.conf file:
rpcuser=yourrpcusername
rpcpassword=yoursecurerpcpassword
rpcbind=127.0.0.1
txindex=1
addnode=5.9.102.210
addnode=78.47.196.146
addnode=178.63.69.164
addnode=88.198.65.74
addnode=5.9.122.241
addnode=144.76.94.38
addnode=89.248.166.91
```
### Create your own Blockchain based on Komodo

Komodo allows anyone to create a runtime fork which represents an independent Blockchain. Below are the detailed instructions:
Setup two independent servers with at least 1 server having a static IP and build komodod on those servers.  

#### On server 1 (with static IP) run:
```shell
./komodod -ac_name=name_of_your_chain -ac_supply=100000 -bind=ip_of_server_1 &
```

#### On server 2 run:
```shell
./komodod -ac_name=name_of_your_chain -ac_supply=100000 -addnode=ip_of_server_1 -gen &
```

**Komodo is based on Zcash which is unfinished and highly experimental.** Use at your own risk.

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
---


Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

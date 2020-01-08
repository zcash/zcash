[![Build Status](https://travis-ci.org/KomodoPlatform/komodo.svg?branch=master)](https://travis-ci.org/KomodoPlatform/komodo)
[![Issues](https://img.shields.io/github/issues-raw/komodoplatform/komodo)](https://github.com/KomodoPlatform/komodo/issues)
[![PRs](https://img.shields.io/github/issues-pr-closed/komodoplatform/komodo)](https://github.com/KomodoPlatform/komodo/pulls)
[![Commits](https://img.shields.io/github/commit-activity/y/komodoplatform/komodo)](https://github.com/KomodoPlatform/komodo/commits/dev)
[![Contributors](https://img.shields.io/github/contributors/komodoplatform/komodo)](https://github.com/KomodoPlatform/komodo/graphs/contributors)
[![Last Commit](https://img.shields.io/github/last-commit/komodoplatform/komodo)](https://github.com/KomodoPlatform/komodo/graphs/commit-activity)


[![gitstars](https://img.shields.io/github/stars/komodoplatform/komodo?style=social)](https://github.com/KomodoPlatform/komodo/stargazers)
[![twitter](https://img.shields.io/twitter/follow/komodoplatform?style=social)](https://twitter.com/komodoplatform)
[![discord](https://img.shields.io/discord/412898016371015680)](https://discord.gg/tvp96Gf)

---
![Komodo Logo](https://i.imgur.com/E8LtkAa.png "Komodo Logo")


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
- API references & Dev Documentation: [https://developers.komodoplatform.com](https://developers.komodoplatform.com/)
- Blog: [https://blog.komodoplatform.com](https://blog.komodoplatform.com/)
- Whitepaper: [Komodo Whitepaper](https://komodoplatform.com/whitepaper)
- Komodo Platform public material: [Komodo Platform public material](https://docs.google.com/document/d/1AbhWrtagu4vYdkl-vsWz-HSNyNvK-W-ZasHCqe7CZy0)

## List of Komodo Platform Technologies

- Delayed Proof of Work (dPoW) - Additional security layer and Komodos own consensus algorithm  
- zk-SNARKs - Komodo Platform's privacy technology for shielded transactions  
- Tokens/Assets Technology - create "colored coins" on the Komodo Platform and use them as a layer for securites  
- Reward API - Komodo CC technology for securities  
- CC - Crypto Conditions to realize "smart contract" logic on top of the Komodo Platform  
- Jumblr - Decentralized tumbler for KMD and other cryptocurrencies  
- Assetchains - Create your own Blockchain that inherits all Komodo Platform functionalities and blockchain interoperability  
- Pegged Assets - Chains that maintain a peg to fiat currencies  
- Peerchains - Scalability solution where sibling chains form a network of blockchains  
- More in depth covered [here](https://docs.google.com/document/d/1AbhWrtagu4vYdkl-vsWz-HSNyNvK-W-ZasHCqe7CZy0)  
- Also note you receive 5% Active User Reward on your balance.  
[See this article for more details](https://support.komodoplatform.com/en/support/solutions/articles/29000024515-how-to-claim-the-kmd-active-user-reward-in-agama)

## Tech Specification
- Max Supply: 200 million KMD
- Block Time: 60 seconds
- Block Reward: 3 KMD
- Mining Algorithm: Equihash

## About this Project
Komodo is based on Zcash and has been extended by our innovative consensus algorithm called dPoW which utilizes Bitcoin's hashrate to store Komodo blockchain information into the Bitcoin blockchain. Other new and native Komodo features are the privacy technology called JUMBLR, our assetchain capabilities (one click plug and play blockchain solutions) and a set of financial decentralization and interoperability technologies. More details are available under https://komodoplatform.com/ and https://blog.komodoplatform.com.

## Getting started

### Dependencies

```shell
#The following packages are needed:
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python python-zmq zlib1g-dev wget libcurl4-gnutls-dev bsdmainutils automake curl libsodium-dev
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
./zcutil/build.sh -j$(expr $(nproc) - 1)
#This can take some time.
```


#### OSX
Ensure you have [brew](https://brew.sh) and Command Line Tools installed.
```shell
# Install brew
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
# Install Xcode, opens a pop-up window to install CLT without installing the entire Xcode package
xcode-select --install 
# Update brew and install dependencies
brew update
brew upgrade
brew tap discoteq/discoteq; brew install flock
brew install autoconf autogen automake
brew update && brew install gcc@8
brew install binutils
brew install protobuf
brew install coreutils
brew install wget
# Clone the Komodo repo
git clone https://github.com/komodoplatform/komodo --branch master --single-branch
# Change master branch to other branch you wish to compile
cd komodo
./zcutil/fetch-params.sh
./zcutil/build-mac.sh -j$(expr $(sysctl -n hw.ncpu) - 1)
# This can take some time.
```

#### Windows
Use a debian cross-compilation setup with mingw for windows and run:
```shell
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python python-zmq zlib1g-dev wget libcurl4-gnutls-dev bsdmainutils automake curl cmake mingw-w64 libsodium-dev libevent-dev
curl https://sh.rustup.rs -sSf | sh
source $HOME/.cargo/env
rustup target add x86_64-pc-windows-gnu
git clone https://github.com/jl777/komodo --branch master --single-branch
cd komodo
./zcutil/fetch-params.sh
./zcutil/build-win.sh -j$(expr $(nproc) - 1)
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
addnode=77.75.121.138
addnode=95.213.238.100
addnode=94.130.148.142
addnode=103.6.12.105
addnode=139.99.209.214
addnode=185.130.212.13
addnode=5.9.142.219
addnode=200.25.4.38
addnode=139.99.136.148

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

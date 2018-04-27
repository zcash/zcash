## Install for Mac OS X

First off you need Apple's Xcode (at least version 7, preferably 8.x or later) and the Xcode Command Line Tools:

https://itunes.apple.com/us/app/xcode/id497799835?mt=12

And Homebrew:

http://brew.sh/

And this is the list of brew packages you'll need installed:

```shell
brew tap discoteq/discoteq; brew install flock
brew install autoconf autogen automake
brew install gcc5
brew install binutils
brew install protobuf
brew install coreutils
brew install wget
```

or 

```shell
brew tap discoteq/discoteq; brew install flock autoconf autogen automake gcc5 binutils protobuf coreutils wget
```

Get all that installed, then run:

```shell
git clone https://github.com/jl777/komodo.git
cd komodo
./zcutil/build-mac.sh
./zcutil/fetch-params.sh
```

To build a distributable version of komodo then run the makeDistrib.sh script after building.

When you are done building, you need to create `Komodo.conf` the Mac way. 

```shell
mkdir ~/Library/Application\ Support/Komodo
touch ~/Library/Application\ Support/Komodo/Komodo.conf
nano ~/Library/Application\ Support/Komodo/Komodo.conf
```

Add the following lines to the Komodo.conf file:

```shell
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

Happy Building

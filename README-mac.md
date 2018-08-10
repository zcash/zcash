
You will need Apple's Xcode (at least version 7, preferably 8.x) and the Xcode Command Line Tools:

https://itunes.apple.com/us/app/xcode/id497799835?mt=12

And Homebrew:

http://brew.sh/

Use the brewfile to install the necessary packages:

```shell
brew bundle
```

or 

```shell
brew tap discoteq/discoteq; brew install flock autoconf autogen automake gcc@6 binutils protobuf coreutils wget
```

Get all that installed, then run:

```shell
git clone https://github.com/VerusCoin/VerusCoin.git
cd VerusCoin
./zcutil/build-mac.sh
./zcutil/fetch-params.sh
```

To build a distributable version of VerusCoin then run the makeReleaseMac.sh script after building. This will fix the dependency references and move the komodod and komodo-cli binaries to the kmd/mac/verus-cli directory along with the 6 libraries required for it to work properly.

When you are done building, you need to create `Komodo.conf` the Mac way. 

```shell
mkdir ~/Library/Application\ Support/Komodo
touch ~/Library/Application\ Support/Komodo/Komodo.conf
nano ~/Library/Application\ Support/Komodo/Komodo.conf
```

Add the following lines to the Komodo.conf file:

```shell
rpcuser=dontuseweakusernameoryougetrobbed
rpcpassword=dontuseweakpasswordoryougetrobbed
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

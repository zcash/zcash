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
```

To build a distributable version of komodo then run the makeDistrib.sh script after building.

When you are done building, you need to do a few things in the [Configuration](https://github.com/zcash/zcash/wiki/1.0-User-Guide#configuration) section of the Zcash User Guide differently because we are on the Mac. All instances of `~/.zcash` need to be replaced by `~/Library/Application\ Support/Zcash` 
The fetch-params.sh script, however, has already been altered to fetch the proving keys into the correct directory to conform to Mac specific naming conventions.

Happy Building

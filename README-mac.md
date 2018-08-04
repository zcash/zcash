You will need Apple's Xcode (at least version 7, preferably 8.x) and the Xcode Command Line Tools:

https://itunes.apple.com/us/app/xcode/id497799835?mt=12

And Homebrew:

http://brew.sh/

Use the brewfile to install the necessary packages:

```shell
brew bundle
```

Get all that installed, then run:

```shell
git clone https://github.com/VerusCoin/VerusCoin.git
cd VerusCoin
./zcutil/build-mac.sh
```

To build a distributable version of VerusCoin then run the makeReleaseMac.sh script after building. This will fix the dependency references and move the komodod and komodo-cli binaries to the kmd/mac/verus-cli directory along with the 6 libraries required for it to work properly.

When you are done building, you need to do a few things in the [Configuration](https://github.com/zcash/zcash/wiki/1.0-User-Guide#configuration) section of the Zcash User Guide differently because we are on the Mac. All instances of `~/.zcash` need to be replaced by `~/Library/Application\ Support/Zcash` 
The fetch-params.sh script, however, has already been altered to fetch the proving keys into the correct directory to conform to Mac specific naming conventions.

Happy Building

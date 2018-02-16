Zclassic v1.0.10-1

NOTICE, the default ports have changed! The p2p port is now 8033 and rpcport is 8023

What is Zclassic?
----------------
Zclassic is financial freedom.

Install
-----------------
### Linux

Get dependencies
```{r, engine='bash'}
sudo apt-get install \
      build-essential pkg-config libc6-dev m4 g++-multilib \
      autoconf libtool ncurses-dev unzip git python \
      zlib1g-dev wget bsdmainutils automake
```

Install
```{r, engine='bash'}
# Build
./zcutil/build.sh -j$(nproc)
# fetch key
./zcutil/fetch-params.sh
# Run
./src/zcashd
```

### Windows
There are two proven ways to build Zclassic for Windows:

* On Linux using [Mingw-w64](https://mingw-w64.org/doku.php) cross compiler tool chain. Ubuntu 16.04 Xenial is proven to work and the instructions is for such release.
* On Windows 10 (64-bit version) using [Windows Subsystem for Linux (WSL)](https://msdn.microsoft.com/commandline/wsl/about) and Mingw-w64 cross compiler tool chain.

With Windows 10, Microsoft released a feature called WSL. It basically allows you to run a bash shell directly on Windows in an ubuntu environment. WSL can be installed with other Linux variants, but as mentioned before, the distro proven to work is Ubuntu.
Follow this [link](https://msdn.microsoft.com/en-us/commandline/wsl/install_guide) for installing WSL first

### Building for Windows 64-Bit
1. Get the usual dependencies:
```{r, engine='bash'}
sudo apt-get install \
      build-essential pkg-config libc6-dev m4 g++-multilib \
      autoconf libtool ncurses-dev unzip git python \
      zlib1g-dev wget bsdmainutils automake
```

2. Get mingw-w64 and set the default ming32 gcc/g++ compiler option to posix

```{r, engine='bash'}
sudo apt install mingw-w64
sudo update-alternatives --config x86_64-w64-mingw32-gcc
sudo update-alternatives --config x86_64-w64-mingw32-g++
```

3. Install Rust
```{r, engine='bash'}
curl https://sh.rustup.rs -sSf | sh
source ~/.cargo/env
rustup install stable-x86_64-unknown-linux-gnu
rustup install stable-x86_64-pc-windows-gnu
rustup target add x86_64-pc-windows-gnu
vi  ~/.cargo/config
```
and add:
```
[target.x86_64-pc-windows-gnu]
linker = "/usr/bin/x86_64-w64-mingw32-gcc"
```

Note that in WSL, the Zclassic source code must be somewhere in the default mount file system. i.e `/usr/src/zclassic`, and not on `/mnt/d/`. What this means is that you cannot build directly on the windows system

4. Build for Windows

```{r, engine='bash'}
PATH=$(echo "$PATH" | sed -e 's/:\/mnt.*//g') # strip out problematic Windows %PATH% imported var
./zcutil/build-win.sh -j$(nproc)
```

5. Installation

After building in WSL, you can make a copy of the compiled executables to a directory on your Windows file system. This is done the following way

```{r, engine='bash'}
make install DESTDIR=/mnt/c/zcl/zclassic
```
This will install the executables to `c:\zcl\zclassic`

### Mac
Get dependencies
```{r, engine='bash'}
#install xcode
xcode-select --install

/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew install cmake autoconf libtool automake coreutils pkgconfig gmp wget

brew install gcc5 --without-multilib
```

Install
```{r, engine='bash'}
# Build
./zcutil/build-mac.sh -j$(sysctl -n hw.ncpu)
# fetch key
./zcutil/fetch-params.sh
# Run
./src/zcashd
```

About
--------------

[Zclassic](http://zclassic.org/), like [Zcash](https://z.cash/), is an implementation of the "Zerocash" protocol.
Based on Bitcoin's code, it intends to offer a far higher standard of privacy
through a sophisticated zero-knowledge proving scheme that preserves
confidentiality of transaction metadata. Technical details are available
in the Zcash [Protocol Specification](https://github.com/zcash/zips/raw/master/protocol/protocol.pdf).

This software is the Zclassic client. It downloads and stores the entire history
of Zclassic transactions; depending on the speed of your computer and network
connection, the synchronization process could take a day or more once the
blockchain has reached a significant size.

Security Warnings
-----------------

See important security warnings in
[doc/security-warnings.md](doc/security-warnings.md).

**Zclassic and Zcash are unfinished and highly experimental.** Use at your own risk.

Deprecation Policy
------------------

This release is considered deprecated 16 weeks after the release day. There
is an automatic deprecation shutdown feature which will halt the node some
time after this 16 week time period. The automatic feature is based on block
height and can be explicitly disabled.

Where do I begin?
-----------------
We have a guide for joining the main Zclassic network:
https://github.com/z-classic/zclassic/wiki/1.0-User-Guide

### Need Help?

* See the documentation at the [Zclassic Wiki](https://github.com/z-classic/zclassic/wiki)
  for help and more information.
* Ask for help on the [Zclassic](http://zcltalk.tech/index.php) forum.

### Want to participate in development?

* Code review is welcome!
* If you want to get to know us join our slack: http://zclassic.herokuapp.com/


Participation in the Zcash project is subject to a
[Code of Conduct](code_of_conduct.md).

Building
--------

Build Zcash along with most dependencies from source by running
./zcutil/build.sh. Currently only Linux is officially supported.

License
-------

For license information see the file [COPYING](COPYING).

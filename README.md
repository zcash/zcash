Zclassic 1.0.3
==============

What is Zclassic?
----------------
Zclassic is a Zcash fork with no 20% Founders Tax.

Get dependencies:
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


About
--------------

[Zclassic]http://zclassic.org/), like [Zcash](https://z.cash/), is an implementation of the "Zerocash" protocol.
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

License
-------

For license information see the file [COPYING](COPYING).

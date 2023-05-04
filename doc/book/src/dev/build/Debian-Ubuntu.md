# Building Zcashd & Zcash-cli on Debian/Ubuntu

*Please upgrade!* [End of Debian Jessie support was on October 1st 2020.](https://forum.zcashcommunity.com/t/end-of-debian-jessie-support-is-on-october-1st-2020/37313)


Zcashd & Zcash-cli are [officially supported](../platform-tier-policy.md) for Debian/Ubuntu. Since Debian/Ubuntu is the best supported platform, we recommend running Zcashd & Zcash-cli on Debian/Ubuntu if possible.

## Instructions

There are multiple ways to download dependencies and build Zcashd & Zcash-cli. We've listed the various ways, in order of recommendation (try the packages first!).

* [Debian Packages Setup](install-debian-bin-packages.md); below video follows these instructions.

  <iframe width="560" height="315" src="https://www.youtube.com/embed/hTKL0jPu7X0" frameborder="0" allow="accelerometer; autoplay; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>

    This is the easiest and most streamlined method.

* Building from source

    This requires downloading dependencies manually via the command line (libtinfo5 is required for v4.1.1):

    ```
     sudo apt-get install \
     build-essential pkg-config libc6-dev m4 g++-multilib \
     autoconf libtool ncurses-dev unzip git python3 python3-zmq \
     zlib1g-dev curl bsdmainutils automake libtinfo5
    ```

    And downloading the source code from the repository:
    ```
     git clone https://github.com/zcash/zcash.git
     cd zcash/
     git checkout v5.5.0
     ./zcutil/fetch-params.sh
    ```

    Then building Zcashd & Zcash-cli:

    ```
    ./zcutil/clean.sh
    ./zcutil/build.sh -j$(nproc)
    ```

    (If you don't have ``nproc``, then substitute the number of cores on your system. If the build runs out of memory, try again without the ``-j`` argument, i.e. just ``./zcutil/build.sh``.)


* [Binary Tarball Download and Setup](install-binary-tarball.md)

    The .tar file unzips into a directory and does not involve a package manager, so it is agnostic of whether that system uses an OS package manager or which one it uses.

## Next steps
Now that you've built Zcashd & Zcash-cli, we can move on to the next steps of: configuration, sync, and use. Refer back to the [Zcashd & Zcash-cli page](https://zcash.readthedocs.io/en/latest/rtd_pages/zcashd.html) for further instructions.

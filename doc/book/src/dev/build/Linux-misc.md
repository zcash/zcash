# Building Zcashd & Zcash-cli on Linux

Zcashd & Zcash-cli are "best effort" supported for non-Debian/Ubuntu Linux OSes.  It's ok to use it and develop on it--we sure do. The level of testing and integration is not up to par with our standards to call this [officially supported](../platform-tier-policy.md#tier-3-platform-policy).

## Instructions

Currently, the only way to install Zcashd & Zcash-cli on Linux is to build from source. Instructions below.

1. Get dependencies by running these commands in Terminal. If already installed, skip steps.

    * Fedora:
      ```
      sudo dnf install \
      git pkgconfig automake autoconf ncurses-devel ncurses-compat-libs \
      python python-zmq curl gtest-devel gcc gcc-c++ libtool \
      patch glibc-static libstdc++-static
      ```
    * RHEL:
      Install devtoolset-3 and autotools-latest (if not previously installed). Run
      `scl enable devtoolset-3 'scl enable autotools-latest bash'` and do the remainder
          of the build in the shell that this starts.
    * CENTOS 7+
      ```
      sudo yum install \
      autoconf libtool unzip git python \
      wget curl  automake gcc gcc-c++ patch \
      glibc-static libstdc++-static
      ```


        And then:
       ```
       sudo yum install centos-release-scl-rh
       sudo yum install devtoolset-3-gcc devtoolset-3-gcc-c++
       sudo update-alternatives --install /usr/bin/gcc-4.9 gcc-4.9 /opt/rh/devtoolset-3/root/usr/bin/gcc 10
       sudo update-alternatives --install /usr/bin/g++-4.9 g++-4.9 /opt/rh/devtoolset-3/root/usr/bin/g++ 10
       scl enable devtoolset-3 bash
       ```

1. Download the source code from the repository:
    ```
     git clone https://github.com/zcash/zcash.git
     cd zcash/
     git checkout v5.5.0
     ./zcutil/fetch-params.sh
     ```
     <!--The message printed by ``git checkout`` about a "detached head" is normal and does not indicate a problem. -->
    This step includes fetching [zcash parameters](https://z.cash/technology/paramgen/), which are numerical dependencies for Zcash a result of the crypto inside. They are around 760 MB in size, so it will take time to download them.

1. Build Zcashd & Zcash-cli
    ```
    ./zcutil/clean.sh
    ./zcutil/build.sh -j$(nproc)
    ```

    If you don't have ``nproc``, then substitute the number of cores on your system. If the build runs out of memory, try again without the ``-j`` argument, i.e. just ``./zcutil/build.sh``

## Next steps
Now that you've built Zcashd & Zcash-cli, we can move on to the next steps of: configuration, sync, and use. Refer back to the [Zcashd & Zcash-cli page](https://zcash.readthedocs.io/en/latest/rtd_pages/zcashd.html) for further instructions.

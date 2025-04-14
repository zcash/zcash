# Zcashd & Zcash-cli on Raspberry Pi

The Raspberry Pi provides a simple, compact computer suitable for running a Zcashd node. The instructions provided here assume you have a Raspberry Pi 4 Model B with the 8GB RAM option and at least a 64GB Micro SD card. Use of a Pi with less RAM or storage space will yield unpredictable results and may result in a failed compile. Given the current Zcash blockchain is >30GB, 64GB is the minimum storage size.

The following instructions will guide you through compiling Zcashd natively on the Raspberry Pi.

Initial Setup
=============

The recommended OS for a Pi-based Zcashd full node is Ubuntu. If you intend to also compile and run zecwallet for the full node, you will want to select the UBuntu Desktop 21.10 (RPi 4/400) 64-bit. Otherwise Ubuntu Server 21.10 (RPi 3/4/400) 64-bit will suffice.


# Install Clean OS
The Raspberry Pi Imager provides a simple interface to select and install the OS using a Windows or Mac. (http://www.raspberry.pi.com/software)

Once the Imager completes the OS write to the Micro SD card, insert the card into the Pi and power it on. It is recommended to have a hardwired Ethernet connection.

The boot sequence will take a few minutes, during which time the root SSH keys will be generated. (NOTE: After the displayed time in seconds passes, pressing the Return key will bring up a login prompt for the default ubuntu user).

At the ``ubuntu:`` prompt, login using the default credentials ubuntu/ubuntu. You will be required to immediately change the password for this account. Be sure to make it complex and store it somewhere safe!

Assuming you've successfully changed the password, the system should display a summary of the operating system version and take you to a ``ubuntu@ubuntu:~$`` prompt.

# Configure Prerequisites

The first step is to prepare the prequisite software for the zcashd compile.

*NOTE: All commands must be run via sudo or from a root command prompt. Use of root carrying additional risk, but might make the process easier. These instructions assume you have superuser rights.*

1. Fully upgrade and update the OS.
    ```
    apt-get upgrade && apt-get update
    ```

2. Install dependencies required for the compiler. After installation, you wil be prompted to restart selected services.

    ```
    apt-get install \
    build-essential pkg-config libc6-dev m4 \
    autoconf libtool libncurses-dev unzip git python3 python3-zmq \
    zlib1g-dev curl bsdmainutils automake libtinfo5
    ```

3. Reboot the Raspberry Pi via ``reboot``.
4. The next step is to install the GCC compiler and the CLANG frontend.

    ```
    apt-get install gcc-10 clang
    ```

    At this point, your Raspberry Pi has all the required components to compile zcashd. It is recommended that you reboot the Pi one more time before proceeding.

5. Clone the zcashd repository and checkout the current code version.

    ```
    git clone https://github.com/zcash/zcash.git
    cd zcash/
    git checkout v5.5.0
    ./zcutil/fetch-params.sh
    ```

6. Build zcashd and zcash-cli.

    ```
    ./zcutil/clean.sh
    ./zcutil/build.sh
    ```

    For build.sh, do not attempt to use the ``-j$(nproc)`` argument. While the Raspberry Pi 4 is a multi-core computer, it does not have enough system memory available to complete a multi-core compile. The entire single-proc compile process will take 3-4 hours.

You now should have a compiled zcashd full node. Once you start the zcashd service, it will take approximately 2 days (48 hours) to fully sync the node.

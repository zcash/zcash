VerusCoin Command Line Tools v0.4.0c
Contents:
komodod - VerusCoin's enhanced Komodo daemon.
komodo-cli - VerusCoin's enhanced Komodo command line utility.
verus - wrapper for komodo-cli that applies the command to the VRSC coin
verusd - wrapper for komodod that sets the VerusCoin parameters to defaults properly
fetch_params.sh - utility to download the zcash parameters needed to start the VerusCoin command line tools and scripts
lib*.dylib - assorted dynamic libraries, dependencies needed by fetch-params.sh, komodod and/or komodo-cli

Command line tools are run from the terminal. You can launch the terminal on a Mac by using the Finder, selecting Applications and from that select Utilities, finally selecting Terminal from the Utilities folder.
You will need to switch to the directory you extracted the verus-cl into. If you extracted it in the Download folder then the change directory command is
cd ~/Downloads/verus-cli
The first time on a new system you will need to run ./fetch-params before using komodod or verusd.

Run ./verusd to launch komodod, and use verus to run commands such as:
./verus stop
Which signals komodod (if it is running) to stop running.

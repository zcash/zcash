VerusCoin Command Line Tools v0.3.3-beta
Contents:
komodod - VerusCoin's enhanced Komodo daemon
komodo-cli - VerusCoin's Komodo command line utility
verus - wrapper for komodo-cli that applies the command to the VRSC coin
verusd - wrapper for komodod that sets the VerusCoin parameters to defaults properly
update-verus-agama.sh - script to update an existing VerusCoin enhanced Agama wallet install with this version of komodod. To update run the script and pass the path to the wallet on the command line: ./update-verus-agama.sh ~/Downloads/Agama-linux-x64

The first time on a new system you will need to run ./fetchparams.sh before using komodod or verusd.
On Ubuntu 18 systems you will need to run these two commands before running the command line tools:
sudo apt-get install libgconf-2-4
sudo apt-get install libcurl3

Run ./verusd to launch komodod, and use ./verus to run commands such as:
./verus stop
Which signals komodod (if it is running) to stop running.

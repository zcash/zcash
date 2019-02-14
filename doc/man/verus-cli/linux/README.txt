VerusCoin Command Line Tools v0.4.0c

Contents:
komodod - VerusCoin's enhanced Komodo daemon
komodo-cli - VerusCoin's Komodo command line utility
verus - wrapper for komodo-cli that applies the command to the VRSC coin
verusd - wrapper for komodod that sets the VerusCoin parameters to defaults properly

The first time on a new system you will need to run ./fetch-params before using komodod or verusd.

Run ./verusd to launch komodod, and use verus to run commands such as:
./verus stop
Which signals komodod (if it is running) to stop running.

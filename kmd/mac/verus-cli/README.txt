VerusCoin Command Line Tools v0.3.3-beta
Contents:
komodod - the Komodo daemon
komodo-cli - Komodo command line utility
verus - wrapper for komodo-cli that applies the command to the VRSC coin
verusd - wrapper for komodod that sets the VerusCoin parameters to defaults properly.

The first time on a new system you will need to run ./fetchparams.sh before using komodod or verusd.
You will need to install xcode from the Apple App Store and run the following two commands before running the command line tools:
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew install gcc@5

If you already have xcode and brew installed you can skip those steps and the same goes for gcc version 5.
Run ./verusd to launch komodod, and use ./verus to run commands such as:
./verus stop
Which signals komodod (if it is running) to stop running.

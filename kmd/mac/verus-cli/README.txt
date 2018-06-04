VerusCoin Command Line Tools v0.3.3-beta
Contents:
Brewfile - configuration for brew that specifies verus-cli requirements. Used via a "brew Brewfile" command. 
komodod - VerusCoin's enhanced Komodo daemon.
komodo-cli - VerusCoin's enhanced Komodo command line utility.
verus - wrapper for komodo-cli that applies the command to the VRSC coin
verusd - wrapper for komodod that sets the VerusCoin parameters to defaults properly
fetch_params.sh - utility to download the zcash parameters needed to start the VerusCoin command line tools and scripts
update-verus-agama.sh - script to update an existing VerusCoin enhanced Agama wallet install with this version of komodod. To update run the script and pass the path to the wallet on the command line: ./update-verus-agama.sh ~/Downloads/Agama-darwin-x64

Command line tools are run from the terminal. You can launch the terminal on a Mac by using the Finder, selecting Applications and from that select Utilities, finally selecting Terminal from the Utilities folder.

Change to the directory you extraced the verus-cli into in the terminal 
cd ~/Downloads/verus-cli
For example, if you extracted it directly in the Downloads directory. 

The first time on a new system you will need to run ./fetchparams.sh before using komodod or verusd.

If you do not have brew installed you will need to install xcode from the Apple App Store and run the following command to get brew installed:
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew install gcc@5
If you already have xcode and brew installed you can skip those.
Now you can simply run
brew Brewfile
and the correct dependencies will be installed for verus-cli to run.

Run ./verusd to launch komodod, and use ./verus to run commands such as:
./verus stop
Which signals komodod (if it is running) to stop running.

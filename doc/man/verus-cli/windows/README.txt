VerusCoin Command Line Tools v0.4.0c
Contents:
komodod.exe - VerusCoin's enhanced Komodo daemon
komodo-cli.exe - VerusCoin's Komodo command line utility
verus.bat - wrapper for komodo-cli that applies the command to the VRSC coin
verusd.bat - wrapper for komodod that sets the VerusCoin parameters to defaults properly

You need to run a command prompt, for example hit <Ctrl><Esc> and type cmd<Enter>
From the command prompt change to the directory where you installed verus-cli. If you downloaded the file to your Downloads directory and extracted it there then the change directory command is
cd \Users\MyName\Downloads\verus-cli
From this directory you can run the Verus command line utilities.
The first time on a new system you will need to run fetch-params before using komodod.exe or verusd.
Many anti-virus products interfere with the VerusCoin tool's ability to open ports and will need to be configured to allow what the scanner says is unsafe behavior.
Extreme cases can result in the virus scanner deleting Agama.exe or moving it to "protect" the system. You will to add the executables to a whitelist and re-extract the verus-cli-windows.zip file if that happens.
Run verusd.bat to launch komodod, and use verus.bat to run commands such as:
verus.bat stop
Which signals komodod.exe (if it is running) to stop running.

Note that if you pass in command line options to verus.bat or verusd.bat that include an = like -ac_veruspos=50 you must surround it with double quotes like this:
verusd.bat "-ac_veruspos=50"
Otherwise Windows will drop the = and pass the two values in as separate command line options.

# Zcashd & Zcash-cli on Windows

We do not currently support Zcashd & Zcash-cli on Windows.

With the changes in Windows 11 and the introduction of the Windows Subsystem for Linux (WSL), it is possible to run the Debian/Ubuntu build through WSL on Windows. This will eliminate the need for a cross-compiled version of Zcashd.

Prerequisite - install Windows Subsystem for Linux. Step-by-step directions are available at http://docs.microsoft.com/en-us/windows/wsl/install

Set the WSL version to 2 with ```wsl --set-default-version 2```

Once complete, you can access the Linux environment by entering  ```wsl``` on a command-line.

Inside the Linux subsystem, follow the same process for installing Zcashd on Debian/Ubuntu (Debian-Ubuntu-build.html).

You are able to start Zcashd from a Windows Desktop icon or scheduled task by referencing the WSL command and the path to the Zcashd binary.

Example:
```wsl //mnt/c/Users/[username]/zcash/src/zcashd --daemon```

will startup Zcashd in background mode. Replace the path as appropriate for your specific installation location.

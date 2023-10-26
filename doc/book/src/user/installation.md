# Installation

## Debian / Ubuntu

The Electric Coin Company operates a package repository for 64-bit Debian-based distributions.

### Fetching a New Signing Key

First install the following dependency so you can talk to our repository using HTTPS:

```
sudo apt-get update && sudo apt-get install apt-transport-https wget gnupg2
```

Next add the Zcash master signing key to apt’s trusted keyring:

```
wget -qO - https://apt.z.cash/zcash.asc | gpg --import
gpg --export B1C9095EAA1848DBB54D9DDA1D05FDC66B372CFE | sudo apt-key add -
```

### Adding and Installing from apt.z.cash

Add the repository to your Bullseye sources:

```
echo "deb [arch=amd64] https://apt.z.cash/ bullseye main" | sudo tee /etc/apt/sources.list.d/zcash.list
```

Update the cache of sources and install Zcash:

```
sudo apt-get update && sudo apt-get install zcash
```

# Troubleshooting

## Different Instruction Sets

Only x86-64 processors are supported.

## Missing sudo

If you’re starting from a new virtual machine, sudo may not come installed. See this issue for instructions to get up and running: https://github.com/zcash/zcash/issues/1844

## libgomp1 or libstdc++6 version problems

These libraries are provided with gcc/g++. If you see errors related to updating them, you may need to upgrade your gcc/g++ packages to version 4.9 or later. First check which version you have using g++ --version; if it is before 4.9 then you will need to upgrade.

On Ubuntu Trusty, you can install gcc/g++ 4.9 as follows:

```
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install g++-4.9
```

## Tor

The repository is also accessible via Tor, after installing the apt-transport-tor package, at the address zcaptnv5ljsxpnjt.onion. Use the following pattern in your sources.list file:

```
deb [arch=amd64] tor+http://zcaptnv5ljsxpnjt.onion/ stretch main
```

## Updating Signing Keys

If your Debian binary package isn’t updating due to an error with the public key, you can resolve the problem by updating to the new key.

## Revoked Key error

If you see:

```
The following signatures were invalid: REVKEYSIG AEFD26F966E279CD
```

Remove the key marked as revoked:

```
sudo apt-key del AEFD26F966E279CD
```

Then [retrieve the latest key](#fetching-a-new-signing-key), then rerun `sudo apt update`.

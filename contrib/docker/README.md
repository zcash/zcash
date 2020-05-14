# zcash Dockerfile

This is an example Dockerfile to run the zcashd daemon in a containerized Debian base OS. The image pulls the apt package built by Electric Coin Company.

The output of building this image should be accessible at https://hub.docker.com/r/electriccoinco/zcashd

## Defaults

The image will run as a non-root user, `zcashd` with uid `2001`. When mapping volumes from the host into the container, these permissions must be used, or rebuild the image with your custom values.

## Run time configuration

The goal is to follow the default zcashd startup behavior as closely as possible.

At startup, the image will execute the [./entrypoint.sh](./entrypoint.sh) script. This script uses environmental variables to configure the command line parameters, do a little house cleaning, and start zcashd.

### Available environment variables

If defined, the value is assigned to the value of the corresponding flag.

```
ZCASHD_NETWORK
ZCASHD_LOGIPS
ZCASHD_EXPERIMENTALFEATURES
ZCASHD_GEN
ZCASHD_ZSHIELDCOINBASE
ZCASHD_RPCUSER
ZCASHD_RPCPASSWORD
ZCASHD_RPCBIND
ZCASHD_ALLOWIP
ZCASHD_TXINDEX
ZCASHD_INSIGHTEXPLORER
ZCASHD_ZMQPORT
ZCASHD_ZMQBIND
```

### Additional configuration

Any provided command line parameters are passed from the entrypoint.sh script to zcashd.

You can skip using environmental variables at all, and instead provide a fully configured `zcash.conf` file and map to `/srv/zcashd/.zcash/zcash.conf` at startup.

## Examples

### See the installed version

This command will create a container, print the version information of the zcashd software installed and then exit and remove the container.

Run
```
docker run --rm electriccoinco/zcashd --version
```

Output
```
Zcash Daemon version v2.1.0-1

In order to ensure you are adequately protecting your privacy when using Zcash,
please see <https://z.cash/support/security/>.

Copyright (C) 2009-2019 The Bitcoin Core Developers
Copyright (C) 2015-2019 The Zcash Developers

This is experimental software.

Distributed under the MIT software license, see the accompanying file COPYING
or <https://www.opensource.org/licenses/mit-license.php>.

This product includes software developed by the OpenSSL Project for use in the
OpenSSL Toolkit <https://www.openssl.org/> and cryptographic software written
by Eric Young.
```

### Persist data to the host

For this example, we'll create a place for zcashd to store the blockchain data, create a new container that uses that location, and run it in the background.

```
mkdir {./zcash-params-dir,./zcash-data-dir}
sudo chown -R 2001.2001 {./zcash-params-dir,./zcash-data-dir}
docker run -d --name my_zcashd \
  -v $(pwd)/zcash-data-dir:/srv/zcashd/.zcash \
  -v $(pwd)/zcash-params-dir/srv/zcashd/.zcash-params \
  electriccoinco/zcashd
```  

Follow the logs to see its activity.

```
docker logs -f my_zcashd
```

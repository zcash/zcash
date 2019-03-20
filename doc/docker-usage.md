# Build a zcashd node from multi-container Docker build

If no VERSION is exported the Dockerfile will apply the default value `master` and build current master.

```
#in the root of the zcash source directory:
export VERSION=v2.0.3
docker build --build-arg VERSION -t zcash/zcash:$VERSION --memory-swap -1 .
```

# Create docker volumes and populate the `zcash-params` volume
```
docker volume create mainnet-params

docker run --rm -itd \
--mount source=mainnet-params,destination=/home/zcash/.zcash-params \
zcash/zcash:latest bash -c "fetch-params.sh"
```

# Create docker volume to persist the blockchain database and state 
```
docker volume create mainnet-chain
```

# Start a container and background it. Optionally mount in a configuration file or wallet
```
CUR_PATH=`PWD`

docker run -itd \
--name zcash \
--mount source=mainnet-chain,destination=/home/zcash/.zcash \
--mount source=mainnet-params,destination=/home/zcash/.zcash-params \
-v $CUR_PATH/mainnet.daemon.conf:/home/zcash/.zcash/zcash.conf \
zcash/zcash:latest
```

# Start the node
```
docker exec -itd zcash bash -c "zcashd"
```

# Query the node
```
docker exec -it zcash zcash-cli getinfo
```

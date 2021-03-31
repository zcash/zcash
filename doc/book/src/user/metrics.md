# zcashd metrics

## Metrics UI

This is the user interface that `zcashd` displays by default when run. It
displays a small selection of interesting metrics, but is not intended for
programmatic consumption.

## RPC methods

`zcashd` provides the following JSON-RPC methods that expose node metrics:

- Chain:
  - `getblockchaininfo`: Various state info regarding block chain processing.
  - `gettxoutsetinfo`: Statistics about the unspent transparent transaction output set.
  - `getmempoolinfo`: Details on the active state of the TX memory pool.
- P2P network:
  - `getnetworkinfo`: Various state info regarding P2P networking.
  - `getpeerinfo`: Data about each connected network node.
  - `getdeprecationinfo`: The current node version and deprecation block height.
- Miscellaneous
  - `getmemoryinfo`: Information about memory usage.
  - `getmininginfo`: Mining-related information.
  - `getinfo` (deprecated): A small subset of the above metrics.

You can see what each method provides with `zcash-cli help METHOD_NAME`.

## Prometheus support

`zcashd` can optionally expose an HTTP server that acts as a Prometheus scrape
endpoint. The server will respond to `GET` requests on any request path.

To enable the endpoint, add `-prometheusport=<port>` to your `zcashd`
configuration (either in `zcash.conf` or on the command line). After
restarting `zcashd` you can then test the endpoint by querying it:

```
$ curl http://127.0.0.1:<port>
# TYPE zcash_net_out_messages counter
zcash_net_out_messages 181

# TYPE zcash_net_in_bytes_total counter
zcash_net_in_bytes_total 3701998

# TYPE zcash_net_in_messages counter
zcash_net_in_messages 184

# TYPE zcashd_build_info counter
zcashd_build_info{version="v4.2.0"} 1

# TYPE zcash_chain_verified_block_total counter
zcash_chain_verified_block_total 162
...
```

By default, access is restricted to localhost. This can be expanded with
`-metricsallowip=<ip>`, which can specify IPs or subnets. Note that HTTPS is not
supported, and therefore connections to the endpoint are not encrypted or
authenticated. Access to the endpoint should be assumed to compromise the
privacy of node operations, by the provided metrics and/or by timing side
channels. Non-localhost access is **strongly discouraged** if the node has a
wallet holding live funds.

### Example metrics collection with Docker

The example instructions below were tested on Windows 10 using Docker Desktop
with the WSL 2 backend:

```
# Create a storage volume for Grafana (once)
docker volume create grafana-storage

# Create a storage volume for Prometheus (once)
docker volume create prometheus-storage

# Run Prometheus
# You will need to modify ~/contrib/metrics/prometheus.yaml to match the
# endpoint configured with -prometheusmetrics (and possibly also for your Docker
# network setup).
docker run --detach -p 9090:9090 --volume prometheus-storage:/prometheus --volume ~/contrib/metrics/prometheus.yaml:/etc/prometheus/prometheus.yml  prom/prometheus

# Run Grafana
docker run --detach -p 3030:3030 --env GF_SERVER_HTTP_PORT=3030 --volume grafana-storage:/var/lib/grafana grafana/grafana
```

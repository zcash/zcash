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
`-metricsallowip=<ip>`, which can specify IPs or subnets. Note that HTTPS is
not supported, and therefore connections to the endpoint are not encrypted or
authenticated. Access to the endpoint should be assumed to compromise the
privacy of node operations, by the provided metrics and/or by timing side
channels. Non-localhost access is **strongly discouraged** if the node has a
wallet holding live funds.

### Metrics collection with Docker

A docker-compose.yml has been provided in `./contrib/metrics` that sets up
local instances of `prometheus` and `grafana` and provides a dashboard that
charts several of the various metrics exposed by `zcashd`'s `prometheus`
endpoint. Note that both `docker` and `docker-compose` must ordinarily be run
with superuser permissions (use `sudo`) when running on Linux.

`docker-compose up`[^1] will start local instances of `prometheus` and `grafana`,
accessible over HTTP on ports `9090` and `3030`, respectively. 

```
cd <zcash_root>/contrib/metrics
docker-compose up -d
```
(substitute the root directory where you have checked out the `zcash` git
repository for `<zcash_root>`)

`~/.zcash/zcash.conf` must be updated to enable `prometheus` and to allow the
`prometheus` server launched via `docker-compose` to connect to the `zcashd`
prometheus endpoint. The following commands can be used to detect the local IP
address for the `prometheus` server and add it to the `~/.zcash/zcash.conf` 
file. 

First, figure out where `prometheus` is running.

```
export PROMETHEUS_DOCKER_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' zcashd-prometheus)
```

Then, update your `~/.zcash/zcash.conf` file to open port `9969` and allow
connections from the `zcashd-prometheus` docker container.

```
cat << PROM_CONF >> ~/.zcash/zcash.conf
prometheusport=9969
metricsallowip=$PROMETHEUS_DOCKER_IP/32
PROM_CONF
```

You may then (re)start `zcashd` and navigate to
[http://localhost:9090/targets?search=](http://localhost:9090/targets?search=)
to verify that the `prometheus` server is able to connect to the `zcashd`
prometheus endpoint; you should see the host
`http://host.docker.internal:9969/` having `UP` status. Once this is working,
navigate to
[http://localhost:3030/d/U4U58t-Gk/zcashd-metrics](http://localhost:3030/d/U4U58t-Gk/zcashd-metrics)
to view the dashboard. The username and password for this local grafana
instance are set by default to `admin`/`admin`; the UI will ask you to change
this on first use. Data gathered by the running prometheus instance and changes
that you make to the grafana interface will be persisted across restarts.

#### Manual Docker Setup Without docker-compose

The example instructions below were tested on Windows 10 using Docker Desktop
with the WSL 2 backend, connected to a `zcashd` running inside WSL2 (but not
inside Docker):

```
# Create a storage volume for Grafana (once)
docker volume create grafana-storage

# Create a storage volume for Prometheus (once)
docker volume create prometheus-storage

# Run Prometheus
# You will need to modify $(zcash_root)/contrib/metrics/prometheus.yaml to match the
# port configured with -prometheusport and -metricsbind / -metricsallowip
# (and possibly also for your Docker network setup).
docker run --detach -p 9090:9090 --volume prometheus-storage:/prometheus --volume $(zcash_root)/contrib/metrics/prometheus.yaml:/etc/prometheus/prometheus.yml  prom/prometheus

# Run Grafana
docker run --detach -p 3030:3030 --env GF_SERVER_HTTP_PORT=3030 --volume grafana-storage:/var/lib/grafana grafana/grafana
```

[^1]: This requires a running Docker daemon. See [the relevant section of the
 Docker Engine manual](https://docs.docker.com/config/daemon/start/).


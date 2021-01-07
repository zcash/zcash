(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Prometheus metrics
------------------

`zcashd` can now be configured to optionally expose an HTTP server that acts as
a Prometheus scrape endpoint. The server will respond to `GET` requests on any
request path.

To enable the endpoint, add `-prometheusport=<port>` to your `zcashd`
configuration (either in `zcash.conf` or on the command line). After
restarting `zcashd` you can then test the endpoint by querying it with e.g.
`curl http://127.0.0.1:<port>`.

By default, access is restricted to localhost. This can be expanded with
`-metricsallowip=<ip>`, which can specify IPs or subnets. Note that HTTPS is not
supported, and therefore connections to the endpoint are not encrypted or
authenticated. Access to the endpoint should be assumed to compromise the
privacy of node operations, by the provided metrics and/or by timing side
channels. Non-localhost access is **strongly discouraged** if the node has a
wallet holding live funds.

The specific metrics names may change in subsequent releases, in particular to
improve interoperability with `zebrad`.

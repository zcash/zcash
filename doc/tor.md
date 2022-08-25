Tor Support in Zcash
====================

Tor can be used to provide a layer of network anonymity for Zcash users.
Additionally, Zcash users may chose to connect only to Tor hidden services, and
also to expose their own Tor hidden service to allow users to connect to them
over the Tor network.

0. Install Tor
--------------

The easiest way to install Tor is to use the
[Tor Browser Bundle](https://www.torproject.org/download/).  For headless
installs, you probably want to install the Tor daemon.  The Tor Project
provides [instructions](https://support.torproject.org/apt/) for doing this on
common Linux distributions.  Note that the Tor Browser Bundle exposes a SOCKS
listener on tcp/9150 by default, while the Tor daemon exposes the SOCKS
listener on tcp/9050.  For the purposes of the example below, we'll assume that
you're using the Tor daemon and that the SOCKS listener is on tcp/9050.

1. Run zcashd over Tor
----------------------

Configuring zcashd to use a Tor SOCKS proxy will route all outgoing connections
over Tor.

```bash
$ zcashd -proxy=127.0.0.1:9050
```

Yay!  Your zcashd node is now leveraging the Tor network to connect to other
zcashd nodes.  But there's more fun to be had.  By creating a
[Tor Hidden Service](https://2019.www.torproject.org/docs/faq.html.en#TorOnionServices).
you can help promote privacy for Zcash users by advertising your node's .onion
address to other Tor Zcash users.

2. Expose your zcashd via a Tor hidden service (optional)
---------------------------------------------------------

Edit your /etc/tor/torrc (or equivalent config file) to map the hidden service
to your zcashd TCP listener.  The directory can be whatever you like but the
port numbers should be equal to the zcashd p2p listen port (8233 by default).
An example is below.

```yaml
############### This section is just for location-hidden services ###

## Once you have configured a hidden service, you can look at the
## contents of the file ".../hidden_service/hostname" for the address
## to tell people.
##
## HiddenServicePort x y:z says to redirect requests on port x to the
## address y:z.

#
# Placeholder for when zcashd adds support for Onion v3 addresses
#HiddenServiceDir /var/lib/tor/zcash_hidden_service_v3/
#HiddenServiceVersion 3
#HiddenServicePort 8233 127.0.0.1:8233

# use the generated v2 Onion hostname until v3 support is complete
HiddenServiceDir /var/lib/tor/zcash_hidden_service_v2/
HiddenServiceVersion 2
HiddenServicePort 8233 127.0.0.1:8233
```

Note that zcashd does not yet support Onion v3 addresses, but will do so before
v2 addresses are removed from Tor.  See [this
issue](https://github.com/zcash/zcash/issues/3051) for more information on
what's required to make zcashd support v3 Onion addresses.

After making these edits to /etc/tor/torrc, restart tor to create the hidden
service hostname and keys.

```bash
$ sudo systemctl restart tor
```

Then set a bash variable to provide your Onion service hostname to zcashd so it
can advertise your node to other Tor capable nodes on the Zcash network.

```bash
$ export MY_ONION_HOSTNAME=`sudo cat /var/lib/tor/zcash_hidden_service_v2/hostname`
```

Now configure the zcashd node to use the Tor proxy, enable the TCP listener
(only on localhost), and advertise your onion address so that other nodes on
the Zcash network can connect to you over Tor.

```bash
$ zcashd -proxy=127.0.0.1:9050 -externalip=$MY_ONION_HOSTNAME -listen -bind=127.0.0.1 -listenonion=0
```
zcashd flags used:

- `-proxy=ip:port`: sets the proxy server. This must match the port IP and port
  on which your Tor listener is configured.
- `-externalip=<ip|host>`: sets the publicly routable address that zcashd will
  advertise to other zcash nodes. This can be an IPv4, IPv6 or .onion address.
  Onion addresses are given preference for advertising and connections. Onionv3
  addresses are [not yet supported](https://github.com/zcash/zcash/issues/3051).
- `-listen`: Enable listening for incoming connections with this flag;
  listening is off by default, but is needed in order for Tor to connect to
  zcashd.
- `-bind=ip`: Bind (only) to this IP.  Will bind to all interfaces by default
  if ``listen=1``.
- `-listenonion=<0|1>`: Enable or disable autoconfiguration of Tor hidden
  service via control socket API.  Disabled in this example because we manually
  configured the hidden service in /etc/tor/torrc.

Once your node is up and running, you can use `zcash-cli` to verify that it
is properly connected to other Zcash nodes over the p2p network, and is
correctly advertising its Onion address to the network.

```bash
$ zcash-cli getnetworkinfo
```

```javascript
{
  "version": 4020050,
  "subversion": "/MagicBean:4.2.0/",
  "protocolversion": 170013,
  "connections": 9,
  "networks": [
    {
    "name": "ipv4",
    "limited": true,
    "reachable": false,
    "proxy": "127.0.0.1:9050",
    "proxy_randomize_credentials": true
    },
    {
    "name": "ipv6",
    "limited": true,
    "reachable": false,
    "proxy": "127.0.0.1:9050",
    "proxy_randomize_credentials": true
    },
    {
    "name": "onion",
    "limited": false,
    "reachable": true,
    "proxy": "127.0.0.1:9050",
    "proxy_randomize_credentials": true
    }
  ],
  "relayfee": 0.00000100,
  "localaddresses": [
    {
    "address": "ynizm2wpla6ec22q.onion",
    "port": 8233,
    "score": 10
    }
  ],
}
```


3. Dynamically Configure Onion Service (Optional)
-------------------------------------------------

Starting with Tor version 0.2.7.1 it is possible, through Tor's control socket
API, to create and destroy 'ephemeral' hidden services programmatically. zcashd
has been updated to make use of this.

This configuration could be used instead of manually configuring the Onion
service as in step 2 above.

If Tor is running (and proper authentication has been configured), zcashd
automatically creates a hidden service to listen on. zcashd will also use Tor
automatically to connect to other .onion nodes if the control socket can be
successfully opened.

This new feature is enabled by default if zcashd is listening (`-listen`) and
requires a Tor connection to work. It can be explicitly disabled with
`-listenonion=0` and, if not disabled, configured using the `-torcontrol`
and `-torpassword` settings. To show verbose debugging information, pass
`-debug=tor`.

Connecting to Tor's control socket API requires one of two authentication
methods to be configured:

1. Cookie authentication, which requires write access to the `CookieAuthFile`
   specified in Tor configuration. In some cases, this is preconfigured and the
   creation of a hidden service is automatic. If permission problems are seen
   with `-debug=tor` they can be resolved by adding both the user running tor
   and  the user running zcashd to the same group and setting permissions
   appropriately. On Debian-based systems the user running zcashd can be added
   to the debian-tor group, which has the appropriate permissions.
2. Authentication with the `-torpassword` flag and a `hash-password`, which
   can be enabled and specified in Tor configuration.

On Debian systems, where Tor is installed via APT, you can trivially permit
zcashd to connect to the Tor socket by adding the zcash user to the
`debian-tor` group.

```bash
$ sudo usermod -aG debian-tor zcash
```

When properly configured, this will allow zcashd to automatically connect to
the Tor control socket API and configure an ephemeral hidden service.

```bash
$ zcashd -debug=tor
```

```
Feb 11 15:26:20.323  INFO main: tor: Got service ID tweustb4j6o3u5x7, advertizing service tweustb4j6o3u5x7.onion:8233
Feb 11 15:26:20.323  DEBUG tor: tor: Cached service private key to /home/zcash/.zcash/onion_private_key
Feb 11 15:26:20.323  INFO main: AddLocal(tweustb4j6o3u5x7.onion:8233,4)
...
Feb 11 15:26:47.565  INFO main: ProcessMessages: advertizing address tweustb4j6o3u5x7.onion:8233
```

4. Connect to a single Zcash Onion server
-----------------------------------------

This invocation will start zcashd and connect via Tor to a single zcashd onion
server.

Launch zcashd as follows:

```bash
$ zcashd -onion=127.0.0.1:9050 -connect=ynizm2wpla6ec22q.onion
```

- `-onion=ip:port`: Use SOCKS5 proxy to reach peers via Tor hidden services.
  This must match the port IP and port on which your Tor listener is
  configured.
- `-connect=<hostname|ip>`: Connect only to the specified node(s); `-noconnect`
  or `-connect=0` alone to disable automatic connections


Now use zcash-cli to verify there is only a single peer connection.

```bash
$ zcash-cli getpeerinfo
```

```javascript
[
  {
    "id": 1,
    "addr": "ynizm2wpla6ec22q.onion",
    ...
    "version": 170013,
    "subver": "/MagicBean:4.2.0/",
    "inbound": false,
    ...
  }
]
```

4. Connect to multiple Zcash Onion servers
------------------------------------------

This invocation will start zcashd, skip DNS seeding, connect via Tor to a
multiple zcashd onion servers, and also advertise your Onion server to other
Tor capable Zcash nodes.

Launch zcashd as follows:

```bash
$ export MY_ONION_HOSTNAME=`sudo cat /var/lib/tor/zcash_hidden_service_v2/hostname`
$ zcashd -listen -onion=127.0.0.1:9050 -addnode=ynizm2wpla6ec22q.onion -dnsseed=0 -onlynet=onion -externalip=$MY_ONION_HOSTNAME -bind=127.0.0.1
```

zcashd flags used:

- `-onion=ip:port`: Use SOCKS5 proxy to reach peers via Tor hidden services.
  This must match the port IP and port on which your Tor listener is
  configured.
- `-addnode=<host|ip>`: Add a node to connect to and attempt to keep the
  connection open
- `-externalip=<ip|onion>`: sets the publicly routable address that zcashd will
  advertise to other zcash nodes. This can be an IPv4, IPv6 or .onion address.
  Onion addresses are given preference for advertising and connections. Onionv3
  addresses are [not yet supported](https://github.com/zcash/zcash/issues/3051).
- `-listen`: Enable listening for incoming connections with this flag;
  listening is off by default, but is needed in order for Tor to connect to
  zcashd.
- `-bind=<ip>`: Bind (only) to this IP.  Will bind to all interfaces by default
  if `listen=1` and `bind` is not set.
- `-onlynet=<net>`: Only connect to nodes in network `<net>` (ipv4, ipv6 or
  onion)




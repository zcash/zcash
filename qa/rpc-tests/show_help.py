#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2017-2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test --help
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, zcashd_binary

from difflib import SequenceMatcher, unified_diff
import subprocess
import tempfile

help_message = """
In order to ensure you are adequately protecting your privacy when using Zcash,
please see <https://z.cash/support/security/>.

Usage:
  zcashd [options]                     Start Zcash Daemon

Options:

  -?
       Print this help message and exit

  -version
       Print version and exit

  -alerts
       Receive and display P2P network alerts (default: 1)

  -alertnotify=<cmd>
       Execute command when a relevant alert is received or we see a really
       long fork (%s in cmd is replaced by message)

  -allowdeprecated=<feature>
       Explicitly allow the use of the specified deprecated feature. Multiple
       instances of this parameter are permitted; values for <feature> must be
       selected from among {"none", "deprecationinfo_deprecationheight",
       "gbt_oldhashes", "z_getbalance", "z_gettotalbalance", "addrtype",
       "getnewaddress", "getrawchangeaddress", "legacy_privacy",
       "wallettxvjoinsplit", "z_getnewaddress", "z_listaddresses"}

  -blocknotify=<cmd>
       Execute command when the best block changes (%s in cmd is replaced by
       block hash)

  -checkblocks=<n>
       How many blocks to check at startup (default: 288, 0 = all)

  -checklevel=<n>
       How thorough the block verification of -checkblocks is (0-4, default: 3)

  -conf=<file>
       Specify configuration file. Relative paths will be prefixed by datadir
       location. (default: zcash.conf)

  -daemon
       Run in the background as a daemon and accept commands

  -datadir=<dir>
       Specify data directory (this path cannot use '~')

  -paramsdir=<dir>
       Specify Zcash network parameters directory

  -dbcache=<n>
       Set database cache size in megabytes (4 to 16384, default: 450)

  -debuglogfile=<file>
       Specify location of debug log file. Relative paths will be prefixed by a
       net-specific datadir location. (default: debug.log)

  -exportdir=<dir>
       Specify directory to be used when exporting data

  -ibdskiptxverification
       Skip transaction verification during initial block download up to the
       last checkpoint height. Incompatible with flags that disable
       checkpoints. (default = 0)

  -loadblock=<file>
       Imports blocks from external blk000??.dat file on startup

  -maxorphantx=<n>
       Keep at most <n> unconnectable transactions in memory (default: 100)

  -par=<n>
       Set the number of script verification threads (IGNORE_NONDETERMINISTIC, 0 = auto, <0 =
       leave that many cores free, default: 0)

  -pid=<file>
       Specify pid file. Relative paths will be prefixed by a net-specific
       datadir location. (default: zcashd.pid)

  -prune=<n>
       Reduce storage requirements by pruning (deleting) old blocks. This mode
       disables wallet support and is incompatible with -txindex. Warning:
       Reverting this setting requires re-downloading the entire blockchain.
       (default: 0 = disable pruning blocks, >550 = target size in MiB to use
       for block files)

  -reindex-chainstate
       Rebuild chain state from the currently indexed blocks (implies -rescan)

  -reindex
       Rebuild chain state and block index from the blk*.dat files on disk
       (implies -rescan)

  -sysperms
       Create new files with system default permissions, instead of umask 077
       (only effective with disabled wallet functionality)

  -txexpirynotify=<cmd>
       Execute command when transaction expires (%s in cmd is replaced by
       transaction id)

  -txindex
       Maintain a full transaction index, used by the getrawtransaction rpc
       call (default: 0)

Connection options:

  -addnode=<ip>
       Add a node to connect to and attempt to keep the connection open

  -banscore=<n>
       Threshold for disconnecting misbehaving peers (default: 100)

  -bantime=<n>
       Number of seconds to keep misbehaving peers from reconnecting (default:
       86400)

  -bind=<addr>
       Bind to given address and always listen on it. Use [host]:port notation
       for IPv6

  -connect=<ip>
       Connect only to the specified node(s); -noconnect or -connect=0 alone to
       disable automatic connections

  -discover
       Discover own IP addresses (default: 1 when listening and no -externalip
       or -proxy)

  -dns
       Allow DNS lookups for -addnode, -seednode and -connect (default: 1)

  -dnsseed
       Query for peer addresses via DNS lookup, if low on addresses (default: 1
       unless -connect/-noconnect)

  -externalip=<ip>
       Specify your own public address

  -forcednsseed
       Always query for peer addresses via DNS lookup (default: 0)

  -listen
       Accept connections from outside (default: 1 if no -proxy or
       -connect/-noconnect)

  -listenonion
       Automatically create Tor hidden service (default: 1)

  -maxconnections=<n>
       Maintain at most <n> connections to peers (default: 125)

  -maxreceivebuffer=<n>
       Maximum per-connection receive buffer, <n>*1000 bytes (default: 5000)

  -maxsendbuffer=<n>
       Maximum per-connection send buffer, <n>*1000 bytes (default: 1000)

  -mempoolevictionmemoryminutes=<n>
       The number of minutes before allowing rejected transactions to re-enter
       the mempool. (default: 60)

  -mempooltxcostlimit=<n>
       An upper bound on the maximum size in bytes of all transactions in the
       mempool. (default: 80000000)

  -onion=<ip:port>
       Use separate SOCKS5 proxy to reach peers via Tor hidden services
       (default: -proxy)

  -onlynet=<net>
       Only connect to nodes in network <net> (ipv4, ipv6 or onion)

  -permitbaremultisig
       Relay non-P2SH multisig (default: 1)

  -peerbloomfilters
       Support filtering of blocks and transaction with bloom filters (default:
       1)

  -port=<port>
       Listen for connections on <port> (default: 8233 or testnet: 18233)

  -proxy=<ip:port>
       Connect through SOCKS5 proxy

  -proxyrandomize
       Randomize credentials for every proxy connection. This enables Tor
       stream isolation (default: 1)

  -seednode=<ip>
       Connect to a node to retrieve peer addresses, and disconnect

  -timeout=<n>
       Specify connection timeout in milliseconds (minimum: 1, default: 5000)

  -torcontrol=<ip>:<port>
       Tor control port to use if onion listening enabled (default:
       127.0.0.1:9051)

  -torpassword=<pass>
       Tor control port password (default: empty)

  -whitebind=<addr>
       Bind to given address and whitelist peers connecting to it. Use
       [host]:port notation for IPv6

  -whitelist=<netmask>
       Whitelist peers connecting from the given netmask or IP address. Can be
       specified multiple times. Whitelisted peers cannot be DoS banned and
       their transactions are always relayed, even if they are already in the
       mempool, useful e.g. for a gateway

  -whitelistrelay
       Accept relayed transactions received from whitelisted inbound peers even
       when not relaying transactions (default: 1)

  -whitelistforcerelay
       Force relay of transactions from whitelisted inbound peers even they
       violate local relay policy (default: 1)

  -maxuploadtarget=<n>
       Tries to keep outbound traffic under the given target (in MiB per 24h),
       0 = no limit (default: 0)

Wallet options:

  -disablewallet
       Do not load the wallet and disable wallet RPC calls

  -keypool=<n>
       Set key pool size to <n> (default: 100)

  -migration
       Enable the Sprout to Sapling migration

  -migrationdestaddress=<zaddr>
       Set the Sapling migration address

  -orchardactionlimit=<n>
       Set the maximum number of Orchard actions permitted in a transaction
       (default 50)

  -paytxfee=<amt>
       The preferred fee rate (in ZEC per 1000 bytes) used for transactions
       created by legacy APIs (sendtoaddress, sendmany, and
       fundrawtransaction). If the transaction is less than 1000 bytes then the
       fee rate is applied as though it were 1000 bytes. When this option is
       not set, the ZIP 317 fee calculation is used.

  -rescan
       Rescan the block chain for missing wallet transactions on startup

  -salvagewallet
       Attempt to recover private keys from a corrupt wallet on startup
       (implies -rescan)

  -spendzeroconfchange
       Spend unconfirmed change when sending transactions (default: 1)

  -txexpirydelta
       Set the number of blocks after which a transaction that has not been
       mined will become invalid (min: 4, default: 20 (pre-Blossom) or 40
       (post-Blossom))

  -upgradewallet
       Upgrade wallet to latest format on startup

  -wallet=<file>
       Specify wallet file absolute path or a path relative to the data
       directory (default: wallet.dat)

  -walletbroadcast
       Make the wallet broadcast transactions (default: 1)

  -walletnotify=<cmd>
       Execute command when a wallet transaction changes (%s in cmd is replaced
       by TxID)

  -zapwallettxes=<mode>
       Delete all wallet transactions and only recover those parts of the
       blockchain through -rescan on startup (1 = keep tx meta data e.g.
       account owner and payment request information, 2 = drop tx meta data)

  -walletrequirebackup=<bool>
       By default, the wallet will not allow generation of new spending keys &
       addresses from the mnemonic seed until the backup of that seed has been
       confirmed with the `zcashd-wallet-tool` utility. A user may start zcashd
       with `-walletrequirebackup=false` to allow generation of spending keys
       even if the backup has not yet been confirmed.

ZeroMQ notification options:

  -zmqpubhashblock=<address>
       Enable publish hash block in <address>

  -zmqpubhashtx=<address>
       Enable publish hash transaction in <address>

  -zmqpubrawblock=<address>
       Enable publish raw block in <address>

  -zmqpubrawtx=<address>
       Enable publish raw transaction in <address>

Monitoring options:

  -metricsallowip=<ip>
       Allow metrics connections from specified source. Valid for <ip> are a
       single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0)
       or a network/CIDR (e.g. 1.2.3.4/24). This option can be specified
       multiple times. (default: only localhost)

  -metricsbind=<addr>
       Bind to given address to listen for metrics connections. (default: bind
       to all interfaces)

  -prometheusport=<port>
       Expose node metrics in the Prometheus exposition format. An HTTP
       listener will be started on <port>, which responds to GET requests on
       any request path. Use -metricsallowip and -metricsbind to control
       access.

  -debugmetrics
       Include debug metrics in exposed node metrics.

Debugging/Testing options:

  -uacomment=<cmt>
       Append comment to the user agent string

  -debug=<category>
       Output debugging information (default: 0, supplying <category> is
       optional). If <category> is not supplied or if <category> = 1, output
       all debugging information. <category> can be: addrman, alert, bench,
       coindb, db, estimatefee, http, libevent, lock, mempool, mempoolrej, net,
       partitioncheck, pow, proxy, prune, rand, receiveunsafe, reindex, rpc,
       selectcoins, tor, zmq, zrpc, zrpcunsafe (implies zrpc). For multiple
       specific categories use -debug=<category> multiple times.

  -experimentalfeatures
       Enable use of experimental features

  -help-debug
       Show all debugging options (usage: --help -help-debug)

  -logips
       Include IP addresses in debug output (default: 0)

  -logtimestamps
       Prepend debug output with timestamp (default: 1)

  -minrelaytxfee=<amt>
       Transactions must have at least this fee rate (in ZEC per 1000 bytes)
       for relaying, mining and transaction creation (default: 0.000001). This
       is not the only fee constraint.

  -maxtxfee=<amt>
       Maximum total fees (in ZEC) to use in a single wallet transaction or raw
       transaction; setting this too low may abort large transactions (default:
       0.10)

  -printtoconsole
       Send trace/debug info to console instead of the debug log

Chain selection options:

  -testnet
       Use the test chain

Node relay options:

  -datacarrier
       Relay and mine data carrier transactions (default: 1)

  -datacarriersize
       Maximum size of data in data carrier transactions we relay and mine
       (default: 83)

Block creation options:

  -blockmaxsize=<n>
       Set maximum block size in bytes (default: 2000000)

  -blockunpaidactionlimit=<n>
       Set the limit on unpaid actions that will be accepted in a block for
       transactions paying less than the ZIP 317 fee (default: 50)

Mining options:

  -gen
       Generate coins (default: 0)

  -genproclimit=<n>
       Set the number of threads for coin generation if enabled (-1 = all
       cores, default: 1)

  -equihashsolver=<name>
       Specify the Equihash solver to be used if enabled (default: "default")

  -mineraddress=<addr>
       Send mined coins to a specific single address

  -minetolocalwallet
       Require that mined blocks use a coinbase address in the local wallet
       (default: 1)

RPC server options:

  -server
       Accept command line and JSON-RPC commands

  -rest
       Accept public REST requests (default: 0)

  -rpcbind=<addr>
       Bind to given address to listen for JSON-RPC connections. Use
       [host]:port notation for IPv6. This option can be specified multiple
       times (default: bind to all interfaces)

  -rpccookiefile=<loc>
       Location of the auth cookie. Relative paths will be prefixed by a
       net-specific datadir location. (default: data dir)

  -rpcuser=<user>
       Username for JSON-RPC connections

  -rpcpassword=<pw>
       Password for JSON-RPC connections

  -rpcauth=<userpw>
       Username and hashed password for JSON-RPC connections. The field
       <userpw> comes in the format: <USERNAME>:<SALT>$<HASH>. A canonical
       python script is included in share/rpcuser. This option can be specified
       multiple times

  -rpcport=<port>
       Listen for JSON-RPC connections on <port> (default: 8232 or testnet:
       18232)

  -rpcallowip=<ip>
       Allow JSON-RPC connections from specified source. Valid for <ip> are a
       single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0)
       or a network/CIDR (e.g. 1.2.3.4/24). This option can be specified
       multiple times

  -rpcthreads=<n>
       Set the number of threads to service RPC calls (default: 4)

Metrics Options (only if -daemon and -printtoconsole are not set):

  -showmetrics
       Show metrics on stdout (default: 1 if running in a console, 0 otherwise)

  -metricsui
       Set to 1 for a persistent metrics screen, 0 for sequential metrics
       output (default: 1 if running in a console, 0 otherwise)

  -metricsrefreshtime
       Number of seconds between metrics refreshes (default: 1 if running in a
       console, 600 otherwise)

Compatibility options:

  -preferredtxversion
       Preferentially create transactions having the specified version when
       possible (default: 4)

"""

class ShowHelpTest(BitcoinTestFramework):

    def setup_network(self):
        self.nodes = []

    def show_help(self):
        with tempfile.SpooledTemporaryFile(max_size=2**16) as log_stdout:
            args = [ zcashd_binary(), "--help" ]
            process = subprocess.run(args, stdout=log_stdout)
            assert_equal(process.returncode, 0)
            log_stdout.seek(0)
            stdout = log_stdout.read().decode('utf-8')
            # Skip the first line which contains version information.
            actual = stdout.split('\n', 1)[1]
            expected = help_message

            changed = False

            for group in SequenceMatcher(None, expected, actual).get_grouped_opcodes():
                # The first and last group are context lines.
                assert_equal(group[0][0], 'equal')
                assert_equal(group[-1][0], 'equal')

                if (
                    len(group) == 3 and
                    group[1][0] == 'replace' and
                    expected[group[1][1]:group[1][2]] == 'IGNORE_NONDETERMINISTIC'
                ):
                    # This is an expected difference, we can ignore it.
                    pass
                else:
                    changed = True
            if changed:
                diff = '\n'.join(unified_diff(
                    expected.split('\n'),
                    actual.split('\n'),
                    'expected',
                    'actual',
                ))
                raise AssertionError('Help text has changed:\n' + diff)

    def run_test(self):
        self.show_help()

if __name__ == '__main__':
    ShowHelpTest().main()

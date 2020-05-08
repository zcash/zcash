# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .


#
# Helpful routines for regression testing
#

# Add python-bitcoinrpc to module search path:
import os
import sys

from binascii import hexlify, unhexlify
from base64 import b64encode
from decimal import Decimal, ROUND_DOWN
import json
import random
import shutil
import subprocess
import tempfile
import time
import re
import errno
from dataclasses import dataclass

from . import coverage
from .authproxy import AuthServiceProxy, JSONRPCException

COVERAGE_DIR = None
PRE_BLOSSOM_BLOCK_TARGET_SPACING = 150
POST_BLOSSOM_BLOCK_TARGET_SPACING = 75

BIP0031_VERSION          = 60000
# These are the testnet protocol versions.
SPROUT_PROTO_VERSION     = 170002  # past bip-31 for ping/pong
OVERWINTER_PROTO_VERSION = 170003
SAPLING_PROTO_VERSION    = 170006
BLOSSOM_PROTO_VERSION    = 170008
HEARTWOOD_PROTO_VERSION  = 170010
CANOPY_PROTO_VERSION     = 170012

SPROUT_VERSION_GROUP_ID     = 0x00000000
OVERWINTER_VERSION_GROUP_ID = 0x03C48270
SAPLING_VERSION_GROUP_ID    = 0x892F2085
# No transaction format change in Blossom or Heartwood.

SPROUT_BRANCH_ID     = 0x00000000
OVERWINTER_BRANCH_ID = 0x5BA81B19
SAPLING_BRANCH_ID    = 0x76B809BB
BLOSSOM_BRANCH_ID    = 0x2BB40E60
HEARTWOOD_BRANCH_ID  = 0xF5B9230B
CANOPY_BRANCH_ID     = 0xE9FF75A6

@dataclass
class Epoch:
    name: str
    branch_id: int
    proto_version: int

EPOCHS = (
    Epoch("Sprout",     SPROUT_BRANCH_ID,     SPROUT_PROTO_VERSION    ),
    Epoch("Overwinter", OVERWINTER_BRANCH_ID, OVERWINTER_PROTO_VERSION),
    Epoch("Sapling",    SAPLING_BRANCH_ID,    SAPLING_PROTO_VERSION   ),
    Epoch("Blossom",    BLOSSOM_BRANCH_ID,    BLOSSOM_PROTO_VERSION   ),
    Epoch("Heartwood",  HEARTWOOD_BRANCH_ID,  HEARTWOOD_PROTO_VERSION ),
)


def enable_coverage(dirname):
    """Maintain a log of which RPC calls are made during testing."""
    global COVERAGE_DIR
    COVERAGE_DIR = dirname


def get_rpc_proxy(url, node_number, timeout=None):
    """
    Args:
        url (str): URL of the RPC server to call
        node_number (int): the node number (or id) that this calls to

    Kwargs:
        timeout (int): HTTP timeout in seconds

    Returns:
        AuthServiceProxy. convenience object for making RPC calls.

    """
    proxy_kwargs = {}
    if timeout is not None:
        proxy_kwargs['timeout'] = timeout

    proxy = AuthServiceProxy(url, **proxy_kwargs)
    proxy.url = url  # store URL on proxy for info

    coverage_logfile = coverage.get_filename(
        COVERAGE_DIR, node_number) if COVERAGE_DIR else None

    return coverage.AuthServiceProxyWrapper(proxy, coverage_logfile)


def p2p_port(n):
    return 11000 + n + os.getpid()%999
def rpc_port(n):
    return 12000 + n + os.getpid()%999

def check_json_precision():
    """Make sure json library being used does not lose precision converting BTC values"""
    n = Decimal("20000000.00000003")
    satoshis = int(json.loads(json.dumps(float(n)))*1.0e8)
    if satoshis != 2000000000000003:
        raise RuntimeError("JSON encode/decode loses precision")

def bytes_to_hex_str(byte_str):
    return hexlify(byte_str).decode('ascii')

def hex_str_to_bytes(hex_str):
    return unhexlify(hex_str.encode('ascii'))

def str_to_b64str(string):
    return b64encode(string.encode('utf-8')).decode('ascii')

def sync_blocks(rpc_connections, wait=1):
    """
    Wait until everybody has the same block count, and has notified
    all internal listeners of them
    """
    while True:
        counts = [ x.getblockcount() for x in rpc_connections ]
        if counts == [ counts[0] ]*len(counts):
            break
        time.sleep(wait)

    # Now that the block counts are in sync, wait for the internal
    # notifications to finish
    while True:
        notified = [ x.getblockchaininfo()['fullyNotified'] for x in rpc_connections ]
        if notified == [ True ] * len(notified):
            break
        time.sleep(wait)

def sync_mempools(rpc_connections, wait=1):
    """
    Wait until everybody has the same transactions in their memory
    pools, and has notified all internal listeners of them
    """
    while True:
        pool = set(rpc_connections[0].getrawmempool())
        num_match = 1
        for i in range(1, len(rpc_connections)):
            if set(rpc_connections[i].getrawmempool()) == pool:
                num_match = num_match+1
        if num_match == len(rpc_connections):
            break
        time.sleep(wait)

    # Now that the mempools are in sync, wait for the internal
    # notifications to finish
    while True:
        notified = [ x.getmempoolinfo()['fullyNotified'] for x in rpc_connections ]
        if notified == [ True ] * len(notified):
            break
        time.sleep(wait)

bitcoind_processes = {}

def initialize_datadir(dirname, n):
    datadir = os.path.join(dirname, "node"+str(n))
    if not os.path.isdir(datadir):
        os.makedirs(datadir)
    with open(os.path.join(datadir, "zcash.conf"), 'w') as f:
        f.write("regtest=1\n")
        f.write("showmetrics=0\n")
        f.write("rpcuser=rt\n")
        f.write("rpcpassword=rt\n")
        f.write("port="+str(p2p_port(n))+"\n")
        f.write("rpcport="+str(rpc_port(n))+"\n")
        f.write("listenonion=0\n")
    return datadir

def rpc_url(i, rpchost=None):
    return "http://rt:rt@%s:%d" % (rpchost or '127.0.0.1', rpc_port(i))

def wait_for_bitcoind_start(process, url, i):
    '''
    Wait for bitcoind to start. This means that RPC is accessible and fully initialized.
    Raise an exception if bitcoind exits during initialization.
    '''
    while True:
        if process.poll() is not None:
            raise Exception('bitcoind exited with status %i during initialization' % process.returncode)
        try:
            rpc = get_rpc_proxy(url, i)
            rpc.getblockcount()
            break # break out of loop on success
        except IOError as e:
            if e.errno != errno.ECONNREFUSED: # Port not yet open?
                raise # unknown IO error
        except JSONRPCException as e: # Initialization phase
            if e.error['code'] != -28: # RPC in warmup?
                raise # unknown JSON RPC exception
        time.sleep(0.25)

def initialize_chain(test_dir):
    """
    Create (or copy from cache) a 200-block-long chain and
    4 wallets.
    """

    # Due to the consensus change fix for the timejacking attack, we need to
    # ensure that the cache is pretty fresh. Specifically, we need the median
    # time past of the chain tip of the cache to be no more than 90 minutes
    # behind the current local time, or else mined blocks will be rejected by
    # all nodes, halting the test. With Sapling active by default, this requires
    # the chain tip itself to be no more than 75 minutes behind the current
    # local time.
    #
    # We address this here, by regenerating the cache if it is more than 60
    # minutes old. This gives 15 minutes of slack initially that an RPC test has
    # to complete in, if it is started right at the oldest cache time. Within an
    # individual test, the first five calls to `generate` will each advance the
    # median time past of the chain tip by 2.5 minutes (with Sapling active by
    # default). Therefore, if the logic between the completion of any two
    # adjacent calls to `generate` within a test takes longer than 2.5 minutes,
    # the excess will subtract from the slack.
    if os.path.isdir(os.path.join("cache", "node0")):
        if os.stat("cache").st_mtime + (60 * 60) < time.time():
            print("initialize_chain(): Removing stale cache")
            shutil.rmtree("cache")

    if (not os.path.isdir(os.path.join("cache","node0"))
        or not os.path.isdir(os.path.join("cache","node1"))
        or not os.path.isdir(os.path.join("cache","node2"))
        or not os.path.isdir(os.path.join("cache","node3"))):

        #find and delete old cache directories if any exist
        for i in range(4):
            if os.path.isdir(os.path.join("cache","node"+str(i))):
                shutil.rmtree(os.path.join("cache","node"+str(i)))

        # Create cache directories, run bitcoinds:
        for i in range(4):
            datadir=initialize_datadir("cache", i)
            args = [ os.getenv("BITCOIND", "bitcoind"), "-keypool=1", "-datadir="+datadir, "-discover=0" ]
            args.extend([
                '-nuparams=5ba81b19:1', # Overwinter
                '-nuparams=76b809bb:1', # Sapling
            ])
            if i > 0:
                args.append("-connect=127.0.0.1:"+str(p2p_port(0)))
            bitcoind_processes[i] = subprocess.Popen(args)
            if os.getenv("PYTHON_DEBUG", ""):
                print("initialize_chain: bitcoind started, waiting for RPC to come up")
            wait_for_bitcoind_start(bitcoind_processes[i], rpc_url(i), i)
            if os.getenv("PYTHON_DEBUG", ""):
                print("initialize_chain: RPC successfully started")

        rpcs = []
        for i in range(4):
            try:
                rpcs.append(get_rpc_proxy(rpc_url(i), i))
            except:
                sys.stderr.write("Error connecting to "+rpc_url(i)+"\n")
                sys.exit(1)

        # Create a 200-block-long chain; each of the 4 nodes
        # gets 25 mature blocks and 25 immature.
        # Blocks are created with timestamps 2.5 minutes apart (matching the
        # chain defaulting above to Sapling active), starting 200 * 2.5 minutes
        # before the current time.
        block_time = int(time.time()) - (200 * PRE_BLOSSOM_BLOCK_TARGET_SPACING)
        for i in range(2):
            for peer in range(4):
                for j in range(25):
                    set_node_times(rpcs, block_time)
                    rpcs[peer].generate(1)
                    block_time += PRE_BLOSSOM_BLOCK_TARGET_SPACING
                # Must sync before next peer starts generating blocks
                sync_blocks(rpcs)
        # Check that local time isn't going backwards
        assert_greater_than(time.time() + 1, block_time)

        # Shut them down, and clean up cache directories:
        stop_nodes(rpcs)
        wait_bitcoinds()
        for i in range(4):
            os.remove(log_filename("cache", i, "debug.log"))
            os.remove(log_filename("cache", i, "db.log"))
            os.remove(log_filename("cache", i, "peers.dat"))
            os.remove(log_filename("cache", i, "fee_estimates.dat"))

    for i in range(4):
        from_dir = os.path.join("cache", "node"+str(i))
        to_dir = os.path.join(test_dir,  "node"+str(i))
        shutil.copytree(from_dir, to_dir)
        initialize_datadir(test_dir, i) # Overwrite port/rpcport in zcash.conf

def initialize_chain_clean(test_dir, num_nodes):
    """
    Create an empty blockchain and num_nodes wallets.
    Useful if a test case wants complete control over initialization.
    """
    for i in range(num_nodes):
        initialize_datadir(test_dir, i)


def _rpchost_to_args(rpchost):
    '''Convert optional IP:port spec to rpcconnect/rpcport args'''
    if rpchost is None:
        return []

    match = re.match('(\[[0-9a-fA-f:]+\]|[^:]+)(?::([0-9]+))?$', rpchost)
    if not match:
        raise ValueError('Invalid RPC host spec ' + rpchost)

    rpcconnect = match.group(1)
    rpcport = match.group(2)

    if rpcconnect.startswith('['): # remove IPv6 [...] wrapping
        rpcconnect = rpcconnect[1:-1]

    rv = ['-rpcconnect=' + rpcconnect]
    if rpcport:
        rv += ['-rpcport=' + rpcport]
    return rv

def start_node(i, dirname, extra_args=None, rpchost=None, timewait=None, binary=None, stderr=None):
    """
    Start a bitcoind and return RPC connection to it
    """
    datadir = os.path.join(dirname, "node"+str(i))
    if binary is None:
        binary = os.getenv("BITCOIND", "bitcoind")
    args = [ binary, "-datadir="+datadir, "-keypool=1", "-discover=0", "-rest" ]
    args.extend([
        '-nuparams=5ba81b19:1', # Overwinter
        '-nuparams=76b809bb:1', # Sapling
    ])
    if extra_args is not None: args.extend(extra_args)
    bitcoind_processes[i] = subprocess.Popen(args, stderr=stderr)
    if os.getenv("PYTHON_DEBUG", ""):
        print("start_node: bitcoind started, waiting for RPC to come up")
    url = rpc_url(i, rpchost)
    wait_for_bitcoind_start(bitcoind_processes[i], url, i)
    if os.getenv("PYTHON_DEBUG", ""):
        print("start_node: RPC successfully started")
    proxy = get_rpc_proxy(url, i, timeout=timewait)

    if COVERAGE_DIR:
        coverage.write_all_rpc_commands(COVERAGE_DIR, proxy)

    return proxy

def assert_start_raises_init_error(i, dirname, extra_args=None, expected_msg=None):
    with tempfile.SpooledTemporaryFile(max_size=2**16) as log_stderr:
        try:
            node = start_node(i, dirname, extra_args, stderr=log_stderr)
            stop_node(node, i)
        except Exception as e:
            assert 'bitcoind exited' in str(e) #node must have shutdown
            if expected_msg is not None:
                log_stderr.seek(0)
                stderr = log_stderr.read().decode('utf-8')
                if expected_msg not in stderr:
                    raise AssertionError("Expected error \"" + expected_msg + "\" not found in:\n" + stderr)
        else:
            if expected_msg is None:
                assert_msg = "bitcoind should have exited with an error"
            else:
                assert_msg = "bitcoind should have exited with expected error " + expected_msg
            raise AssertionError(assert_msg)

def start_nodes(num_nodes, dirname, extra_args=None, rpchost=None, binary=None):
    """
    Start multiple bitcoinds, return RPC connections to them
    """
    if extra_args is None: extra_args = [ None for i in range(num_nodes) ]
    if binary is None: binary = [ None for i in range(num_nodes) ]
    rpcs = []
    try:
        for i in range(num_nodes):
            rpcs.append(start_node(i, dirname, extra_args[i], rpchost, binary=binary[i]))
    except: # If one node failed to start, stop the others
        stop_nodes(rpcs)
        raise
    return rpcs

def log_filename(dirname, n_node, logname):
    return os.path.join(dirname, "node"+str(n_node), "regtest", logname)

def check_node(i):
    bitcoind_processes[i].poll()
    return bitcoind_processes[i].returncode

def stop_node(node, i):
    node.stop()
    bitcoind_processes[i].wait()
    del bitcoind_processes[i]

def stop_nodes(nodes):
    for node in nodes:
        node.stop()
    del nodes[:] # Emptying array closes connections as a side effect

def set_node_times(nodes, t):
    for node in nodes:
        node.setmocktime(t)

def wait_bitcoinds():
    # Wait for all bitcoinds to cleanly exit
    for bitcoind in list(bitcoind_processes.values()):
        bitcoind.wait()
    bitcoind_processes.clear()

def connect_nodes(from_connection, node_num):
    ip_port = "127.0.0.1:"+str(p2p_port(node_num))
    from_connection.addnode(ip_port, "onetry")
    # poll until version handshake complete to avoid race conditions
    # with transaction relaying
    while any(peer['version'] == 0 for peer in from_connection.getpeerinfo()):
        time.sleep(0.1)

def connect_nodes_bi(nodes, a, b):
    connect_nodes(nodes[a], b)
    connect_nodes(nodes[b], a)

def find_output(node, txid, amount):
    """
    Return index to output of txid with value amount
    Raises exception if there is none.
    """
    txdata = node.getrawtransaction(txid, 1)
    for i in range(len(txdata["vout"])):
        if txdata["vout"][i]["value"] == amount:
            return i
    raise RuntimeError("find_output txid %s : %s not found"%(txid,str(amount)))


def gather_inputs(from_node, amount_needed, confirmations_required=1):
    """
    Return a random set of unspent txouts that are enough to pay amount_needed
    """
    assert(confirmations_required >=0)
    utxo = from_node.listunspent(confirmations_required)
    random.shuffle(utxo)
    inputs = []
    total_in = Decimal("0.00000000")
    while total_in < amount_needed and len(utxo) > 0:
        t = utxo.pop()
        total_in += t["amount"]
        inputs.append({ "txid" : t["txid"], "vout" : t["vout"], "address" : t["address"] } )
    if total_in < amount_needed:
        raise RuntimeError("Insufficient funds: need %d, have %d"%(amount_needed, total_in))
    return (total_in, inputs)

def make_change(from_node, amount_in, amount_out, fee):
    """
    Create change output(s), return them
    """
    outputs = {}
    amount = amount_out+fee
    change = amount_in - amount
    if change > amount*2:
        # Create an extra change output to break up big inputs
        change_address = from_node.getnewaddress()
        # Split change in two, being careful of rounding:
        outputs[change_address] = Decimal(change/2).quantize(Decimal('0.00000001'), rounding=ROUND_DOWN)
        change = amount_in - amount - outputs[change_address]
    if change > 0:
        outputs[from_node.getnewaddress()] = change
    return outputs

def send_zeropri_transaction(from_node, to_node, amount, fee):
    """
    Create&broadcast a zero-priority transaction.
    Returns (txid, hex-encoded-txdata)
    Ensures transaction is zero-priority by first creating a send-to-self,
    then using its output
    """

    # Create a send-to-self with confirmed inputs:
    self_address = from_node.getnewaddress()
    (total_in, inputs) = gather_inputs(from_node, amount+fee*2)
    outputs = make_change(from_node, total_in, amount+fee, fee)
    outputs[self_address] = float(amount+fee)

    self_rawtx = from_node.createrawtransaction(inputs, outputs)
    self_signresult = from_node.signrawtransaction(self_rawtx)
    self_txid = from_node.sendrawtransaction(self_signresult["hex"], True)

    vout = find_output(from_node, self_txid, amount+fee)
    # Now immediately spend the output to create a 1-input, 1-output
    # zero-priority transaction:
    inputs = [ { "txid" : self_txid, "vout" : vout } ]
    outputs = { to_node.getnewaddress() : float(amount) }

    rawtx = from_node.createrawtransaction(inputs, outputs)
    signresult = from_node.signrawtransaction(rawtx)
    txid = from_node.sendrawtransaction(signresult["hex"], True)

    return (txid, signresult["hex"])

def random_zeropri_transaction(nodes, amount, min_fee, fee_increment, fee_variants):
    """
    Create a random zero-priority transaction.
    Returns (txid, hex-encoded-transaction-data, fee)
    """
    from_node = random.choice(nodes)
    to_node = random.choice(nodes)
    fee = min_fee + fee_increment*random.randint(0,fee_variants)
    (txid, txhex) = send_zeropri_transaction(from_node, to_node, amount, fee)
    return (txid, txhex, fee)

def random_transaction(nodes, amount, min_fee, fee_increment, fee_variants):
    """
    Create a random transaction.
    Returns (txid, hex-encoded-transaction-data, fee)
    """
    from_node = random.choice(nodes)
    to_node = random.choice(nodes)
    fee = min_fee + fee_increment*random.randint(0,fee_variants)

    (total_in, inputs) = gather_inputs(from_node, amount+fee)
    outputs = make_change(from_node, total_in, amount, fee)
    outputs[to_node.getnewaddress()] = float(amount)

    rawtx = from_node.createrawtransaction(inputs, outputs)
    signresult = from_node.signrawtransaction(rawtx)
    txid = from_node.sendrawtransaction(signresult["hex"], True)

    return (txid, signresult["hex"], fee)

def assert_equal(expected, actual, message=""):
    if expected != actual:
        if message:
            message = "; %s" % message 
        raise AssertionError("(left == right)%s\n  left: <%s>\n right: <%s>" % (message, str(expected), str(actual)))

def assert_true(condition, message = ""):
    if not condition:
        raise AssertionError(message)
        
def assert_false(condition, message = ""):
    assert_true(not condition, message)

def assert_greater_than(thing1, thing2):
    if thing1 <= thing2:
        raise AssertionError("%s <= %s"%(str(thing1),str(thing2)))

def assert_raises(exc, fun, *args, **kwds):
    try:
        fun(*args, **kwds)
    except exc:
        pass
    except Exception as e:
        raise AssertionError("Unexpected exception raised: "+type(e).__name__)
    else:
        raise AssertionError("No exception raised")

def fail(message=""):
    raise AssertionError(message)


# Returns an async operation result
def wait_and_assert_operationid_status_result(node, myopid, in_status='success', in_errormsg=None, timeout=300):
    print('waiting for async operation {}'.format(myopid))
    result = None
    for _ in range(1, timeout):
        results = node.z_getoperationresult([myopid])
        if len(results) > 0:
            result = results[0]
            break
        time.sleep(1)

    assert_true(result is not None, "timeout occurred")
    status = result['status']

    debug = os.getenv("PYTHON_DEBUG", "")
    if debug:
        print('...returned status: {}'.format(status))

    errormsg = None
    if status == "failed":
        errormsg = result['error']['message']
        if debug:
            print('...returned error: {}'.format(errormsg))
        assert_equal(in_errormsg, errormsg)

    assert_equal(in_status, status, "Operation returned mismatched status. Error Message: {}".format(errormsg))

    return result


# Returns txid if operation was a success or None
def wait_and_assert_operationid_status(node, myopid, in_status='success', in_errormsg=None, timeout=300):
    result = wait_and_assert_operationid_status_result(node, myopid, in_status, in_errormsg, timeout)
    if result['status'] == "success":
        return result['result']['txid']
    else:
        return None

# Find a coinbase address on the node, filtering by the number of UTXOs it has.
# If no filter is provided, returns the coinbase address on the node containing
# the greatest number of spendable UTXOs.
# The default cached chain has one address per coinbase output.
def get_coinbase_address(node, expected_utxos=None):
    addrs = [utxo['address'] for utxo in node.listunspent() if utxo['generated']]
    assert(len(set(addrs)) > 0)

    if expected_utxos is None:
        addrs = [(addrs.count(a), a) for a in set(addrs)]
        return sorted(addrs, reverse=True)[0][1]

    addrs = [a for a in set(addrs) if addrs.count(a) == expected_utxos]
    assert(len(addrs) > 0)
    return addrs[0]

def check_node_log(self, node_number, line_to_check, stop_node = True):
    print("Checking node " + str(node_number) + " logs")
    if stop_node:
        self.nodes[node_number].stop()
        bitcoind_processes[node_number].wait()
    logpath = self.options.tmpdir + "/node" + str(node_number) + "/regtest/debug.log"
    with open(logpath, "r") as myfile:
        logdata = myfile.readlines()
    for (n, logline) in enumerate(logdata):
        if line_to_check in logline:
            return n
    raise AssertionError(repr(line_to_check) + " not found")

def nuparams(branch_id, height):
    return '-nuparams=%x:%d' % (branch_id, height)

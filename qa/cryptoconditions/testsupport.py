from __future__ import print_function
import sys
import json
import time
import copy
import base64
import logging
import functools
import subprocess


class RPCError(IOError):
    pass


class JsonClient(object):
    def __getattr__(self, method):
        if method[0] == '_':
            return getattr(super(JsonClient, self), method)
        def inner(*args):
            return self._exec(method, args)
        return inner

    def load_response(self, data):
        data = json.loads(data.decode("utf-8"))
        if data.get('error'):
            raise RPCError(data['error'])
        if 'result' in data:
            return data['result']
        return data


def run_cmd(cmd):
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    assert proc.wait() == 0, cmd
    return proc.stdout.read()


def to_hex(s):
    return base64.b16encode(s).decode('utf-8')


class Hoek(JsonClient):
    def _exec(self, method, args):
        cmd = ['hoek', method, json.dumps(args[0])]
        return self.load_response(run_cmd(cmd))


class Komodod(JsonClient):
    def _exec(self, method, args):
        if not hasattr(self, '_url'):
            urltpl = 'http://$rpcuser:$rpcpassword@${rpchost:-127.0.0.1}:$rpcport'
            cmd = ['bash', '-c', '. $KOMODO_CONF_PATH && echo -n %s' % urltpl]
            self._url = run_cmd(cmd)

        req = {'method': method, 'params': args, 'id': 1}
        cmd = ['curl', '-s', '-H', 'Content-Type: application/json', '-d', json.dumps(req), self._url]
        return self.load_response(run_cmd(cmd))


rpc = Komodod()
hoek = Hoek()


def wait_for_block(height):
    logging.info("Waiting for block height %s" % height)
    for i in range(100):
        try:
            return rpc.getblock(str(height))
        except RPCError as e:
            time.sleep(1)
    raise Exception('Time out waiting for block at height %s' % height)


def sign(tx):
    signed = hoek.signTxBitcoin({'tx': tx, 'privateKeys': [notary_sk]})
    signed = hoek.signTxEd25519({'tx': signed, 'privateKeys': [alice_sk, bob_sk]})
    return hoek.signTxSecp256k1({'tx': signed, 'privateKeys': [notary_sk]})


def submit(tx):
    encoded = hoek.encodeTx(tx)
    try:
        rpc.getrawtransaction(encoded['txid'])
        logging.info("Transaction already in chain: %s" % encoded['txid'])
        return encoded['txid']
    except RPCError:
        pass
    logging.info("submit transaction: %s:%s" % (encoded['txid'], json.dumps(tx)))
    return rpc.sendrawtransaction(encoded['hex'])


def get_fanout_txid():
    block = wait_for_block(1)
    reward_txid = block['tx'][0]
    reward_tx_raw = rpc.getrawtransaction(reward_txid)
    reward_tx = hoek.decodeTx({'hex': reward_tx_raw})
    balance = reward_tx['outputs'][0]['amount']

    n_outs = 40
    remainder = balance - n_outs * 1000

    fanout = {
        'inputs': [
            {'txid': reward_txid, 'idx': 0, 'script': {'pubkey': notary_pk}}
        ],
        "outputs": (n_outs * [
            {"amount": 1000, "script": {"condition": cond_alice}}
        ] + [{"amount": remainder, 'script': {'address': notary_addr}}])
    }

    txid = submit(sign(fanout))
    rpc.getrawtransaction(txid)
    return txid


def fanout_input(n):
    def decorate(f):
        def wrapper():
            if not hasattr(fanout_input, 'txid'):
                fanout_input.txid = get_fanout_txid()
            inp = {'txid': fanout_input.txid, 'idx': n, 'script': {'fulfillment': cond_alice}}
            f(copy.deepcopy(inp))
        return functools.wraps(f)(wrapper)
    return decorate


def decode_base64(data):
    """Decode base64, padding being optional.

    :param data: Base64 data as an ASCII byte string
    :returns: The decoded byte string.
    """
    missing_padding = len(data) % 4
    if missing_padding:
        data += '=' * (4 - missing_padding)
    return base64.urlsafe_b64decode(data)


def encode_base64(data):
    return base64.urlsafe_b64encode(data).rstrip(b'=')


notary_addr = 'RXSwmXKtDURwXP7sdqNfsJ6Ga8RaxTchxE'
notary_pk = '0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47'
notary_sk = 'UxFWWxsf1d7w7K5TvAWSkeX4H95XQKwdwGv49DXwWUTzPTTjHBbU'
alice_pk = '8ryTLBMnozUK4xUz7y49fjzZhxDDMK7c4mucLdbVY6jW'
alice_sk = 'E4ER7uYvaSTdpQFzTXNNSTkR6jNRJyqhZPJMGuU899nY'
cond_alice = {"type": "ed25519-sha-256", "publicKey": alice_pk}
bob_pk = 'C8MfEjKiFxDguacHvcM7MV83cRQ55RAHacC73xqg8qeu'
bob_sk = 'GrP1fZdUxUc1NYmu7kiNkJV4p7PKpshp1yBY7hogPUWT'
cond_bob = {"type": "ed25519-sha-256", "publicKey": bob_pk}
nospend = {"amount": 1000, "script": {"address": notary_addr}}

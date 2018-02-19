import sys
import json
import time
import argparse
import subprocess


class RPCError(IOError):
    pass


class JsonClient(object):
    def __getattr__(self, method):
        def inner(*args):
            return self._exec(method, args)
        return inner

    def load_response(self, data):
        data = json.loads(data)
        if data.get('error'):
            raise RPCError(data['error'])
        if 'result' in data:
            return data['result']
        return data


def run_cmd(cmd):
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    #print >>sys.stderr, "> %s" % repr(cmd)[1:-1]
    assert proc.wait() == 0
    return proc.stdout.read()


class Hoek(JsonClient):
    def _exec(self, method, args):
        cmd = ['hoek', method, json.dumps(args[0])]
        return self.load_response(run_cmd(cmd))


class Komodod(JsonClient):
    def _exec(self, method, args):
        req = {'method': method, 'params': args, 'id': 1}
        cmd = ['curl', '-s', '-H', 'Content-Type: application/json',
               '-d', json.dumps(req), CONFIG['komodod_url']]
        return self.load_response(run_cmd(cmd))


rpc = Komodod()
hoek = Hoek()


def wait_for_block(height):
    print >>sys.stderr, "Waiting for block height %s" % height
    for i in range(100):
        try:
            return rpc.getblock(str(height))
        except RPCError as e:
            time.sleep(3)
    raise Exception('Time out waiting for block at height %s' % height)


def sign_and_submit(tx):
    signed = hoek.signTxBitcoin({'tx': tx, 'privateKeys': [notary_sk]})
    signed = hoek.signTxEd25519({'tx': signed['tx'], 'privateKeys': [alice_sk]})
    encoded = hoek.encodeTx(signed)
    try:
        rpc.getrawtransaction(encoded['txid'])
        print >> sys.stderr, "Transaction already in chain: %s" % encoded['txid']
        return encoded['txid']
    except RPCError:
        pass
    print >> sys.stderr, "submit transaction: %s:%s" % (encoded['txid'], json.dumps(signed))
    print encoded
    return rpc.sendrawtransaction(encoded['hex'])



CONFIG = None
FANOUT_TXID = None


def setup():
    global CONFIG, FANOUT_TXID
    if not CONFIG:
        parser = argparse.ArgumentParser(description='Crypto-Condition integration suite.')
        parser.add_argument('komodod_conf', help='Location of Komodod config file')
        args = parser.parse_args()

        urltpl = 'http://$rpcuser:$rpcpassword@${rpchost:-127.0.0.1}:$rpcport'
        cmd = ['bash', '-c', '. %s && echo -n %s' % (args.komodod_conf, urltpl)]
        url = run_cmd(cmd)
        
        CONFIG = {'komodod_url': url}

    if not FANOUT_TXID:
        block = wait_for_block(1)
        reward_txid = block['tx'][0]
        reward_tx_raw = rpc.getrawtransaction(reward_txid)
        reward_tx = hoek.decodeTx({'hex': reward_tx_raw})
        balance = reward_tx['tx']['outputs'][0]['amount']

        n_outs = 100
        remainder = balance - n_outs * 1000

        fanout = {
            'inputs': [
                {'txid': reward_txid, 'idx': 0, 'script': {'pubkey': notary_pk}}
            ],
            "outputs": (100 * [
                {"amount": 1000, "script": {"address": notary_addr}}
            ] + [{"amount": remainder, 'script': {'address': notary_addr}}])
        }

        FANOUT_TXID = sign_and_submit(fanout)

    return FANOUT_TXID


notary_addr = 'RXSwmXKtDURwXP7sdqNfsJ6Ga8RaxTchxE'
notary_pk = '0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47'
notary_sk = 'UxFWWxsf1d7w7K5TvAWSkeX4H95XQKwdwGv49DXwWUTzPTTjHBbU'
alice_pk = '8ryTLBMnozUK4xUz7y49fjzZhxDDMK7c4mucLdbVY6jW'
alice_sk = 'E4ER7uYvaSTdpQFzTXNNSTkR6jNRJyqhZPJMGuU899nY'
cond_alice = {"type": "ed25519-sha-256", "publicKey": alice_pk}

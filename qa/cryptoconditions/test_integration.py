import sys
import time
import json
from testsupport import *


def wait_for_block(height):
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
        return encoded['txid']
    except RPCError:
        pass
    print >> sys.stderr, "submit transaction: %s:%s" % (encoded['txid'], json.dumps(signed))
    return rpc.sendrawtransaction(encoded['hex'])


def test_basic_spend():
    block = wait_for_block(3)
    reward_txid = block['tx'][0]
    reward_tx_raw = rpc.getrawtransaction(reward_txid)
    reward_tx = hoek.decodeTx({'hex': reward_tx_raw})
    balance = reward_tx['tx']['outputs'][0]['amount']

    spend0 = {
        'inputs': [
            {'txid': reward_txid, 'idx': 0, 'script': {'pubkey': notary_pk}}
        ],
        "outputs": [
            {"amount": 1000, "script": {"condition": cond_alice}},
            {"amount": balance - 1000, "script": {"address": notary_addr}}
        ]
    }

    spend0_txid = sign_and_submit(spend0)

    spend1 = {
        'inputs': [
            {'txid': spend0_txid, 'idx': 0, 'script': {"fulfillment": cond_alice}},
            {'txid': spend0_txid, 'idx': 1, 'script': {'address': notary_addr}}
        ],
        'outputs': [
            {"amount": balance, "script": {"address": notary_addr}}
        ]
    }

    spend1_txid = sign_and_submit(spend1)

    assert rpc.getrawtransaction(spend1_txid)

    print("all done!")


notary_addr = 'RXSwmXKtDURwXP7sdqNfsJ6Ga8RaxTchxE'
notary_pk = '0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47'
notary_sk = 'UxFWWxsf1d7w7K5TvAWSkeX4H95XQKwdwGv49DXwWUTzPTTjHBbU'
alice_pk = '8ryTLBMnozUK4xUz7y49fjzZhxDDMK7c4mucLdbVY6jW'
alice_sk = 'E4ER7uYvaSTdpQFzTXNNSTkR6jNRJyqhZPJMGuU899nY'
cond_alice = {"type": "ed25519-sha-256", "publicKey": alice_pk}

       

if __name__ == '__main__':
    hoek = Hoek()
    rpc = Komodod('/home/scott/.komodo/CCWAT/CCWAT.conf')
    test_basic_spend()


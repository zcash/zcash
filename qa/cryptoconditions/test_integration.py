import sys
import time
import json
import logging
import binascii
from testsupport import *


@fanout_input(0)
def test_basic_spend(inp):
    spend = {'inputs': [inp], "outputs": [nospend]}
    spend_txid = submit(sign(spend))
    assert rpc.getrawtransaction(spend_txid)


@fanout_input(1)
def test_fulfillment_wrong_signature(inp):
    # Set other pubkey and sign
    inp['script']['fulfillment']['publicKey'] = bob_pk
    spend = {'inputs': [inp], 'outputs': [nospend]}
    signed = sign(spend)

    # Set the correct pubkey, signature is wrong
    signed['tx']['inputs'][0]['script']['fulfillment']['publicKey'] = alice_pk

    try:
        assert not submit(signed), 'should raise an error'
    except RPCError as e:
        assert 'Script evaluated without error but finished with a false/empty top stack element' in str(e), str(e)


@fanout_input(2)
def test_fulfillment_wrong_pubkey(inp):
    spend = {'inputs': [inp], 'outputs': [nospend]}

    signed = sign(spend)
    # Set the wrong pubkey, signature is correct
    signed['tx']['inputs'][0]['script']['fulfillment']['publicKey'] = bob_pk

    try:
        assert not submit(signed), 'should raise an error'
    except RPCError as e:
        assert 'Script evaluated without error but finished with a false/empty top stack element' in str(e), str(e)


@fanout_input(3)
def test_fulfillment_invalid_fulfillment(inp):
    # Create a valid script with an invalid fulfillment payload
    inp['script'] = binascii.hexlify(b"\007invalid").decode('utf-8')
    spend = {'inputs': [inp], 'outputs': [nospend]}

    try:
        assert not submit({'tx': spend}), 'should raise an error'
    except RPCError as e:
        assert 'Crypto-Condition payload is invalid' in str(e), str(e)


if __name__ == '__main__':
    for name, f in globals().items():
        if name.startswith('test_'):
            logging.info("Running test: %s" % name)
            f()

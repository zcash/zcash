import sys
import time
import json
import logging
import binascii
from testsupport import *


@fanout_input(0)
def test_basic_spend(inp):
    spend0 = {
        'inputs': [inp],
        "outputs": [
            {"amount": 500, "script": {"condition": cond_alice}},
            {"amount": 500, "script": {"address": notary_addr}}
        ]
    }
    spend0_txid = submit(sign(spend0))
    spend1 = {
        'inputs': [
            {'txid': spend0_txid, 'idx': 0, 'script': {"fulfillment": cond_alice}},
            {'txid': spend0_txid, 'idx': 1, 'script': {'address': notary_addr}}
        ],
        'outputs': [{"amount": 1000, "script": {"address": notary_addr}}]
    }
    spend1_txid = submit(sign(spend1))
    assert rpc.getrawtransaction(spend1_txid)


@fanout_input(1)
def test_fulfillment_wrong_signature(inp):
    spend0 = {
        'inputs': [inp],
        "outputs": [{"amount": 1000, "script": {"condition": cond_bob}}]
    }
    spend0_txid = submit(sign(spend0))
    spend1 = {
        'inputs': [{'txid': spend0_txid, 'idx': 0, 'script': {"fulfillment": cond_alice}}],
        'outputs': [{"amount": 1000, "script": {"address": notary_addr}}]
    }

    signed = sign(spend1)
    # Set the correct pubkey, signature is wrong
    signed['tx']['inputs'][0]['script']['fulfillment']['publicKey'] = bob_pk

    try:
        assert not submit(sign(spend1)), 'should raise an error'
    except RPCError as e:
        assert '16: mandatory-script-verify-flag-failed' in str(e), str(e)


@fanout_input(2)
def test_fulfillment_wrong_pubkey(inp):
    spend0 = {
        'inputs': [inp],
        "outputs": [{"amount": 1000, "script": {"condition": cond_alice}}]
    }
    spend0_txid = submit(sign(spend0))
    spend1 = {
        'inputs': [{'txid': spend0_txid, 'idx': 0, 'script': {"fulfillment": cond_alice}}],
        'outputs': [{"amount": 1000, "script": {"address": notary_addr}}]
    }

    signed = sign(spend1)
    # Set the wrong pubkey, signature is correct
    signed['tx']['inputs'][0]['script']['fulfillment']['publicKey'] = bob_pk

    try:
        assert not submit(signed), 'should raise an error'
    except RPCError as e:
        assert '16: mandatory-script-verify-flag-failed' in str(e), str(e)


@fanout_input(3)
def test_fulfillment_invalid_fulfillment(inp):
    spend0 = {
        'inputs': [inp],
        "outputs": [{"amount": 1000, "script": {"condition": cond_alice}}]
    }
    spend0_txid = submit(sign(spend0))

    # Create a valid script with an invalid fulfillment payload
    script = binascii.hexlify(b"\007invalid").decode('utf-8')

    spend1 = {
        'inputs': [{'txid': spend0_txid, 'idx': 0, 'script': script}],
        'outputs': [{"amount": 1000, "script": {"address": notary_addr}}]
    }

    try:
        assert not submit({'tx': spend1}), 'should raise an error'
    except RPCError as e:
        assert 'Crypto-Condition payload is invalid' in str(e), str(e)


if __name__ == '__main__':
    for name, f in globals().items():
        if name.startswith('test_'):
            logging.info("Running test: %s" % name)
            f()

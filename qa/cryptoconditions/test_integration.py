import sys
import time
import json
import logging
import binascii
import struct
from testsupport import *


SCRIPT_FALSE = 'Script evaluated without error but finished with a false/empty top stack element'


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

    # Set the correct pubkey, signature is bob's
    signed['inputs'][0]['script']['fulfillment']['publicKey'] = alice_pk

    try:
        assert not submit(signed), 'should raise an error'
    except RPCError as e:
        assert SCRIPT_FALSE in str(e), str(e)


@fanout_input(2)
def test_fulfillment_wrong_pubkey(inp):
    spend = {'inputs': [inp], 'outputs': [nospend]}
    signed = sign(spend)

    # Set the wrong pubkey, signature is correct
    signed['inputs'][0]['script']['fulfillment']['publicKey'] = bob_pk

    try:
        assert not submit(signed), 'should raise an error'
    except RPCError as e:
        assert SCRIPT_FALSE in str(e), str(e)


@fanout_input(3)
def test_invalid_fulfillment_binary(inp):
    # Create a valid script with an invalid fulfillment payload
    inp['script'] = binascii.hexlify(b"\007invalid").decode('utf-8')
    spend = {'inputs': [inp], 'outputs': [nospend]}

    try:
        assert not submit(spend), 'should raise an error'
    except RPCError as e:
        assert 'Crypto-Condition payload is invalid' in str(e), str(e)


@fanout_input(4)
def test_invalid_condition(inp):
    # Create a valid output script with an invalid cryptocondition binary
    outputscript = to_hex(b"\007invalid\xcc")
    spend = {'inputs': [inp], 'outputs': [{'amount': 1000, 'script': outputscript}]}
    spend_txid = submit(sign(spend))

    spend1 = {
        'inputs': [{'txid': spend_txid, 'idx': 0, 'script': {'fulfillment': cond_alice}}],
        'outputs': [nospend],
    }

    try:
        assert not submit(sign(spend1)), 'should raise an error'
    except RPCError as e:
        assert SCRIPT_FALSE in str(e), str(e)


@fanout_input(5)
def test_oversize_fulfillment(inp):
    # Create oversize fulfillment script where the total length is <2000
    binscript = b'\x4d%s%s' % (struct.pack('h', 2000), b'a' * 2000)
    inp['script'] = to_hex(binscript)
    spend = {'inputs': [inp], 'outputs': [nospend]}

    try:
        assert not submit(spend), 'should raise an error'
    except RPCError as e:
        assert 'scriptsig-size' in str(e), str(e)


@fanout_input(6)
def test_aux_basic(inp):
    aux_cond = {
        'type': 'aux-sha-256',
        'method': 'equals',
        'conditionAux': 'LTE',
        'fulfillmentAux': 'LTE'
    }

    # Setup some aux outputs
    spend0 = {
        'inputs': [inp],
        'outputs': [
            {'amount': 500, 'script': {'condition': aux_cond}},
            {'amount': 500, 'script': {'condition': aux_cond}}
        ]
    }
    spend0_txid = submit(sign(spend0))
    assert rpc.getrawtransaction(spend0_txid)

    # Test a good fulfillment
    spend1 = {
        'inputs': [{'txid': spend0_txid, 'idx': 0, 'script': {'fulfillment': aux_cond}}],
        'outputs': [{'amount': 500, 'script': {'condition': aux_cond}}]
    }
    spend1_txid = submit(sign(spend1))
    assert rpc.getrawtransaction(spend1_txid)

    # Test a bad fulfillment
    aux_cond['fulfillmentAux'] = 'WYW'
    spend2 = {
        'inputs': [{'txid': spend0_txid, 'idx': 1, 'script': {'fulfillment': aux_cond}}],
        'outputs': [{'amount': 500, 'script': {'condition': aux_cond}}]
    }
    try:
        assert not submit(sign(spend2)), 'should raise an error'
    except RPCError as e:
        assert SCRIPT_FALSE in str(e), str(e)


@fanout_input(7)
def test_aux_complex(inp):
    aux_cond = {
        'type': 'aux-sha-256',
        'method': 'inputIsReturn',
        'conditionAux': '',
        'fulfillmentAux': 'AQ' # \1 (tx.vout[1])
    }

    # Setup some aux outputs
    spend0 = {
        'inputs': [inp],
        'outputs': [
            {'amount': 500, 'script': {'condition': aux_cond}},
            {'amount': 500, 'script': {'condition': aux_cond}}
        ]
    }
    spend0_txid = submit(sign(spend0))
    assert rpc.getrawtransaction(spend0_txid)

    # Test a good fulfillment
    spend1 = {
        'inputs': [{'txid': spend0_txid, 'idx': 0, 'script': {'fulfillment': aux_cond}}],
        'outputs': [
            {'amount': 250, 'script': {'condition': aux_cond}},
            {'amount': 250, 'script': "6A0B68656C6C6F207468657265"} # OP_RETURN somedata
        ]
    }
    spend1_txid = submit(sign(spend1))
    assert rpc.getrawtransaction(spend1_txid)

    # Test a bad fulfillment
    spend2 = {
        'inputs': [{'txid': spend0_txid, 'idx': 1, 'script': {'fulfillment': aux_cond}}],
        'outputs': [
            {'amount': 500, 'script': "6A0B68656C6C6F207468657265"} # OP_RETURN somedata
        ]
    }
    try:
        assert not submit(sign(spend2)), 'should raise an error'
    except RPCError as e:
        assert SCRIPT_FALSE in str(e), str(e)


@fanout_input(8)
def test_secp256k1_condition(inp):
    ec_cond = {
        'type': 'secp256k1-sha-256',
        'publicKey': notary_pk
    }

    # Create some secp256k1 outputs
    spend0 = {
        'inputs': [inp],
        'outputs': [
            {'amount': 500, 'script': {'condition': ec_cond}},
            {'amount': 500, 'script': {'condition': ec_cond}}
        ]
    }
    spend0_txid = submit(sign(spend0))
    assert rpc.getrawtransaction(spend0_txid)

    # Test a good fulfillment
    spend1 = {
        'inputs': [{'txid': spend0_txid, 'idx': 0, 'script': {'fulfillment': ec_cond}}],
        'outputs': [{'amount': 500, 'script': {'condition': ec_cond}}]
    }
    spend1_txid = submit(sign(spend1))
    assert rpc.getrawtransaction(spend1_txid)

    # Test a bad fulfillment
    spend2 = {
        'inputs': [{'txid': spend0_txid, 'idx': 1, 'script': {'fulfillment': ec_cond}}],
        'outputs': [{'amount': 500, 'script': {'condition': ec_cond}}]
    }
    signed = sign(spend2)
    signed['inputs'][0]['script']['fulfillment']['publicKey'] = \
            '0275cef12fc5c49be64f5aab3d1fbba08cd7b0d02908b5112fbd8504218d14bc7d'
    try:
        assert not submit(signed), 'should raise an error'
    except RPCError as e:
        assert SCRIPT_FALSE in str(e), str(e)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    for name, f in globals().items():
        if name.startswith('test_'):
            logging.info("Running test: %s" % name)
            f()
            logging.info("Test OK: %s" % name)

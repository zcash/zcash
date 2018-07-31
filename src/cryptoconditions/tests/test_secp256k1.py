import json
import base64
import hashlib
import secp256k1
from .test_vectors import jsonRPC


key = secp256k1.PrivateKey()


def test_sign_secp256k1_pass_simple():
    pubkey = encode_b16(key.pubkey.serialize())
    msg = ''
    res = jsonRPC('signTreeSecp256k1', {
        'condition': {
            'type': 'secp256k1-sha-256',
            'publicKey': pubkey,
        },
        'privateKey': encode_b16(key.private_key),
        'message': msg,
    })

    sig = encode_b16(key.ecdsa_serialize_compact(key.ecdsa_sign(msg.encode())))

    assert res == {
        "num_signed": 1,
        "condition": {
            "type": "secp256k1-sha-256",
            "publicKey": pubkey,
            "signature": sig
        }
    }
    cond_bin = jsonRPC('encodeCondition', res['condition'])['bin']
    ffill_bin = jsonRPC('encodeFulfillment', res['condition'])['fulfillment']
    assert jsonRPC('verifyFulfillment', {
        'fulfillment': ffill_bin,
        'message': msg,
        'condition': cond_bin
    }) == {'valid': True}



def test_sign_secp256k1_fail():
    # privateKey doesnt match publicKey
    pubkey = encode_b16(key.pubkey.serialize())
    msg = ''
    res = jsonRPC('signTreeSecp256k1', {
        'condition': {
            'type': 'secp256k1-sha-256',
            'publicKey': pubkey,
        },
        'privateKey': encode_b16(b'0' * 32),
        'message': msg,
    })

    assert res == {
        "num_signed": 0,
        "condition": {
            "type": "secp256k1-sha-256",
            "publicKey": pubkey,
        }
    }


def encode_b16(s):
    return base64.b16encode(s).decode()

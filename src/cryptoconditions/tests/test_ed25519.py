import json
import base64
from .test_vectors import jsonRPC


def test_sign_ed25519_pass_simple():
    res = jsonRPC('signTreeEd25519', {
        'condition': {
            'type': 'ed25519-sha-256',
            'publicKey': "E0x0Ws4GhWhO_zBoUyaLbuqCz6hDdq11Ft1Dgbe9y9k",
        },
        'privateKey': 'D75A980182B10AB7D54BFED3C964073A0EE172F3DAA62325AF021A68F707511A',
        'message': '',
    })

    assert res == {
        "num_signed": 1,
        "condition": {
            "type": "ed25519-sha-256",
            "publicKey": "E0x0Ws4GhWhO_zBoUyaLbuqCz6hDdq11Ft1Dgbe9y9k",
            "signature": "jcuovSRpHwqiC781KzSM1Jd0Qtyfge0cMGttUdLOVdjJlSBFLTtgpinASOaJpd-VGjhSGWkp1hPWuMAAZq6pAg"
        }
    }


def test_sign_ed25519_pass_prefix():
    res = jsonRPC('signTreeEd25519', {
        'condition': {
            'type': 'prefix-sha-256',
            'prefix': 'YmJi',
            'maxMessageLength': 3,
            'subfulfillment': {
                'type': 'ed25519-sha-256',
                'publicKey': "E0x0Ws4GhWhO_zBoUyaLbuqCz6hDdq11Ft1Dgbe9y9k",
            }
        },
        'privateKey': 'D75A980182B10AB7D54BFED3C964073A0EE172F3DAA62325AF021A68F707511A',
        'message': '',
    })

    assert res == {
        "num_signed": 1,
        'condition': {
            'type': 'prefix-sha-256',
            'prefix': 'YmJi',
            'maxMessageLength': 3,
            'subfulfillment': {
                'type': 'ed25519-sha-256',
                'publicKey': "E0x0Ws4GhWhO_zBoUyaLbuqCz6hDdq11Ft1Dgbe9y9k",
                'signature': '4Y6keUFEl4KgIum9e3MBdlhlp32FRas-1N1vhtdy5q3JEPdqMvmXo2Rb99fC6j_3vflh8_QtOEW5rj4utjMOBg',
            }
        },
    }


def test_sign_ed25519_fail():
    # privateKey doesnt match publicKey
    res = jsonRPC('signTreeEd25519', {
        'condition': {
            'type': 'ed25519-sha-256',
            'publicKey': "E0x0Ws4GhWhO_zBoUyaLbuqCz6hDdq11Ft1Dgbe9y9k",
        },
        'privateKey': '225A980182B10AB7D54BFED3C964073A0EE172F3DAA62325AF021A68F707511A',
        'message': '',
    })

    assert res == {
        "num_signed": 0,
        "condition": {
            "type": "ed25519-sha-256",
            "publicKey": "E0x0Ws4GhWhO_zBoUyaLbuqCz6hDdq11Ft1Dgbe9y9k",
        }
    }

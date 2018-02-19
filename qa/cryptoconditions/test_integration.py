import sys
import time
import json
from testsupport import *


def test_basic_spend():
    spend0 = {
        'inputs': [
            {'txid': fanout, 'idx': 0, 'script': {'address': notary_addr}}
        ],
        "outputs": [
            {"amount": 500, "script": {"condition": cond_alice}},
            {"amount": 500, "script": {"address": notary_addr}}
        ]
    }

    spend0_txid = sign_and_submit(spend0)

    spend1 = {
        'inputs': [
            {'txid': spend0_txid, 'idx': 0, 'script': {"fulfillment": cond_alice}},
            {'txid': spend0_txid, 'idx': 1, 'script': {'address': notary_addr}}
        ],
        'outputs': [
            {"amount": 1000, "script": {"address": notary_addr}}
        ]
    }

    spend1_txid = sign_and_submit(spend1)

    assert rpc.getrawtransaction(spend1_txid)

    print("all done!")



if __name__ == '__main__':
    fanout = setup()
    test_basic_spend()

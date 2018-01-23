import os
import subprocess
import json
import base64


inputs = [
    {
        'txid': '9b907ef1e3c26fc71fe4a4b3580bc75264112f95050014157059c736f0202e71',
        'vout': 0,
        'fulfillment': {
            'type': 'preimage-sha-256',
            'preimage': '',
        },
    }
]

outputs = [
    {
        "condition": {
            'type': 'preimage-sha-256',
            'preimage': ''
        },
        "amount": 1,
    }
]

addresses = ['RQygCQA7ovCrVCHM3sSkDQjjQUg5X4SCJw']
privkeys = ["UszeEw39HcoeRegZb74xuvngbcQ5jkwQZZVpAvBp1bvBhXD36ghX"]


proc = subprocess.Popen(['src/komodo-cli', 'createrawtransactioncc', json.dumps(inputs), json.dumps(outputs)],
                        stdout=subprocess.PIPE)

assert not proc.wait()

tx_hex = proc.stdout.read().strip()
tx = base64.b16decode(tx_hex, casefold=True)

print repr(tx)

proc = subprocess.Popen(['src/komodo-cli', 'signrawtransactioncc', tx_hex, json.dumps(privkeys)],
                        stdout=subprocess.PIPE)

proc.wait()

result = json.loads(proc.stdout.read())

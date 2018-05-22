#!/usr/bin/env python

import sys
import json
import ctypes
import base64
import os.path
import argparse
from ctypes import *


so = cdll.LoadLibrary('.libs/libcryptoconditions.so')
so.jsonRPC.restype = c_char_p


def jsonRPC(method, params, load=True):
    out = so.cc_jsonRPC(json.dumps({
        'method': method,
        'params': params,
    }))
    return json.loads(out) if load else out


def b16_to_b64(b16):
    return base64.urlsafe_b64encode(base64.b16decode(b16)).rstrip('=')


USAGE = "cryptoconditions [-h] {method} {request_json}"

def get_help():
    methods = jsonRPC("listMethods", {})['methods']

    txt = USAGE + "\n\nmethods:\n"
    
    for method in methods:
        txt += '    %s: %s\n' % (method['name'], method['description'])

    txt += """\noptional arguments:
  -h, --help            show this help message and exit
"""
    return txt


def get_parser():
    class Parser(argparse.ArgumentParser):
        def format_help(self):
            return get_help()

    parser = Parser(description='Crypto Conditions JSON interface', usage=USAGE)

    json_loads = lambda r: json.loads(r)
    json_loads.__name__ = 'json'
    
    parser.add_argument("method")
    parser.add_argument("request", type=json_loads)
    
    return parser


if __name__ == '__main__':
    args = get_parser().parse_args()
    print(jsonRPC(args.method, args.request, load=False))

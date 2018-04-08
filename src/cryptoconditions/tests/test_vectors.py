import json
import ctypes
import base64
import pytest
import os.path
from ctypes import *


v0000 = '0000_test-minimal-preimage'
v0001 = '0001_test-minimal-prefix'
v0002 = '0002_test-minimal-threshold'
v0003 = '0003_test-minimal-rsa'
v0004 = '0004_test-minimal-ed25519'
v0005 = '0005_test-basic-preimage'
v0006 = '0006_test-basic-prefix'
v0007 = '0007_test-basic-prefix-two-levels-deep'
v0010 = '0010_test-basic-threshold-same-fulfillment-twice'
v0015 = '0015_test-basic-ed25519'
v0016 = '0016_test-advanced-notarized-receipt'
v0017 = '0017_test-advanced-notarized-receipt-multiple-notaries'

# These contain RSA conditions which are not implemented yet
#v0008 = '0008_test-basic-threshold'
#v0009 = '0009_test-basic-threshold-same-condition-twice'
#v0011 = '0011_test-basic-threshold-two-levels-deep'
#v0012 = '0012_test-basic-threshold-schroedinger'
#v0013 = '0013_test-basic-rsa'
#v0014 = '0014_test-basic-rsa4096'

# Custom test vectors
v1000 = '1000_test-minimal-eval'
v1001 = '1001_test-minimal-secp256k1'


all_vectors = {v0000, v0001, v0002, v0004, v0005, v0006, v0007, v0010,
               v0015, v0016, v0017, v1000, v1001}


@pytest.mark.parametrize('vectors_file', all_vectors)
def test_encodeCondition(vectors_file):
    vectors = _read_vectors(vectors_file)
    response = jsonRPC('encodeCondition', vectors['json'])
    assert response == {
        'uri': vectors['conditionUri'],
        'bin': vectors['conditionBinary'],
    }


@pytest.mark.parametrize('vectors_file', all_vectors)
def test_encodeFulfillment(vectors_file):
    vectors = _read_vectors(vectors_file)
    response = jsonRPC('encodeFulfillment', vectors['json'])
    assert response == {
        'fulfillment': vectors['fulfillment'],
    }


@pytest.mark.parametrize('vectors_file', all_vectors)
def test_verifyFulfillment(vectors_file):
    vectors = _read_vectors(vectors_file)
    req = {
        'fulfillment': vectors['fulfillment'],
        'message': vectors['message'],
        'condition': vectors['conditionBinary'],
    }
    assert jsonRPC('verifyFulfillment', req) == {'valid': True}


@pytest.mark.parametrize('vectors_file', all_vectors)
def test_decodeFulfillment(vectors_file):
    vectors = _read_vectors(vectors_file)
    response = jsonRPC('decodeFulfillment', {
        'fulfillment': vectors['fulfillment'],
    })
    assert response == {
        'uri': vectors['conditionUri'],
        'bin': vectors['conditionBinary'],
    }


@pytest.mark.parametrize('vectors_file', all_vectors)
def test_decodeCondition(vectors_file):
    vectors = _read_vectors(vectors_file)
    response = jsonRPC('decodeCondition', {
        'bin': vectors['conditionBinary'],
    })
    assert response['uri'] == vectors['conditionUri']


@pytest.mark.parametrize('vectors_file', all_vectors)
def test_json_condition_json_parse(vectors_file):
    vectors = _read_vectors(vectors_file)
    err = ctypes.create_string_buffer(100)
    cc = so.cc_conditionFromJSONString(json.dumps(vectors['json']).encode(), err)
    out_ptr = so.cc_conditionToJSONString(cc)
    out = ctypes.cast(out_ptr, c_char_p).value.decode()
    assert json.loads(out) == vectors['json']


def b16_to_b64(b16):
    return base64.urlsafe_b64encode(base64.b16decode(b16)).rstrip('=')


def decode_base64(data):
    """Decode base64, padding being optional.

    :param data: Base64 data as an ASCII byte string
    :returns: The decoded byte string.
    """
    missing_padding = len(data) % 4
    if missing_padding:
        data += '=' * (4 - missing_padding)
    return base64.urlsafe_b64decode(data)


def encode_base64(data):
    if type(data) == str:
        data = data.encode()
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode()


def b16_to_b64(b16):
    if type(b16) == str:
        b16 = b16.encode()
    return encode_base64(base64.b16decode(b16))


def b64_to_b16(b64):
    #if type(b64) == str:
    #    b64 = b64.encode()
    return base64.b16encode(decode_base64(b64)).decode()


def _read_vectors(name):
    path = 'tests/vectors/%s.json' % name
    if os.path.isfile(path):
        return json.load(open(path))
    raise IOError("Vectors file not found: %s.json" % name)


so = cdll.LoadLibrary('.libs/libcryptoconditions.so')
so.cc_jsonRPC.restype = c_char_p



def jsonRPC(method, params):
    req = json.dumps({
        'method': method,
        'params': params,
    })
    out = so.cc_jsonRPC(req.encode())
    return json.loads(out.decode())

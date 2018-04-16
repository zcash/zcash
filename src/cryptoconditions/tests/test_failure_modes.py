import json
import ctypes
import base64
from .test_vectors import jsonRPC, so, decode_base64 as d64, encode_base64 as e64


'''
These tests are aimed at edge cases of serverside deployment.

As such, the main functions to test are decoding and verifying fulfillment payloads.
'''

cc_rfb = lambda f: so.cc_readFulfillmentBinary(f, len(f))
cc_rcb = lambda f: so.cc_readConditionBinary(f, len(f))

unhex = lambda s: base64.b16decode(s.upper())


def test_decode_valid_fulfillment():
    f = unhex('a42480206ee12ed43d7dce6fc0b2be20e6808380baafc03d400404bbf95165d7527b373a8100')
    assert cc_rfb(f)


def test_decode_invalid_fulfillment():
    # This fulfillment payload has an invalid type ID but is otherwise valid
    invalid_type_id = unhex('bf632480206ee12ed43d7dce6fc0b2be20e6808380baafc03d400404bbf95165d7527b373a8100')
    assert cc_rfb(invalid_type_id) == 0
    assert cc_rfb('\0') == 0
    assert cc_rfb('') == 0


def test_large_fulfillment():
    # This payload is valid and very large
    f = unhex("a1839896b28083989680") + 10000000 * b'e' + \
         unhex("81030186a0a226a42480206ee12ed43d7dce6fc0b2be20e68083"
               "80baafc03d400404bbf95165d7527b373a8100")
    cond = cc_rfb(f)
    buflen = len(f) + 1000  # Wiggle room
    buf = ctypes.create_string_buffer(buflen)
    assert so.cc_fulfillmentBinary(cond, buf, buflen)
    so.cc_free(cond)


def test_decode_valid_condition():
    # Valid preimage
    assert cc_rcb(d64(b'oCWAIMqXgRLKG73K-sIxs5oj3E2nhu_4FHxOcrmAd4Wv7ki7gQEB'))

    # Somewhat bogus condition (prefix with no subtypes) but nonetheless valid structure
    assert cc_rcb(unhex("a10a8001618101ff82020700"))


def test_decode_invalid_condition():
    assert 0 == cc_rcb("")
    assert 0 == cc_rcb("\0")

    # Bogus type ID
    assert 0 == cc_rcb(unhex('bf630c80016181030186a082020700'))


def test_validate_empty_sigs():
    #// TODO: test failure mode: empty sig / null pointer
    pass


def test_non_canonical_secp256k1():
    cond = {
        "type": "secp256k1-sha-256",
        "publicKey": "02D5D969305535AC29A77079C11D4F0DD40661CF96E04E974A5E8D7E374EE225AA",
        # Signature is correct, but non canonical; validation should fail
        "signature": "9C2D6FF39F340BBAF65E884BDFFAE7436A7B7568839C5BA117FA6818245FB9DAC32138302B2C208E6E424DD9C702790AE10B241B89C8D0A06B01CB708334AB6E"
    }
    res = jsonRPC('verifyFulfillment', {
        'fulfillment': jsonRPC('encodeFulfillment', cond)['fulfillment'],
        'condition': jsonRPC('encodeCondition', cond)['bin'],
        'message': ''
        })
    assert res['valid'] == False


def test_malleability_checked():
    assert     cc_rfb(b'\xa2\x13\xa0\x0f\xa0\x05\x80\x03abc\xa0\x06\x80\x04abcd\xa1\x00')
    assert not cc_rfb(b'\xa2\x13\xa0\x0f\xa0\x06\x80\x04abcd\xa0\x05\x80\x03abc\xa1\x00')


so.cc_conditionUri.restype = ctypes.c_char_p

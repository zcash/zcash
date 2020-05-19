import pytest
import time
import sys
from random import choice
from string import ascii_uppercase
try:
    from slickrpc import Proxy
except ImportError:
    from bitcoinrpc.authproxy import AuthServiceProxy as Proxy


def assert_success(result):
    assert result['result'] == 'success'


 
def assert_error(result):
    """In some cases CCs throwing exception such as:
    {'error': {'code': -3, 'message': 'Amount out of range'}
    And in some cases errors responses forming manually, e.g.
    { "result" : "error", "error" : "something" }
    lets handle both options
    """
    
    if 'result' in result.keys():
        assert result['result'] == 'error'
    else:
        assert 'error' in result.keys()

def mine_and_waitconfirms(txid, proxy, confs_req=2):  # should be used after tx is send
    # we need the tx above to be confirmed in the next block
    attempts = 0
    while True:
        try:
            confirmations_amount = proxy.getrawtransaction(txid, 1)['confirmations']
            if confirmations_amount < confs_req:
                print("\ntx is not confirmed yet! Let's wait a little more")
                time.sleep(5)
            else:
                print("\ntx confirmed")
                return True
        except KeyError as e:
            print("\ntx is in mempool still probably, let's wait a little bit more\nError: ", e)
            time.sleep(5)
            attempts += 1
            if attempts < 100:
                pass
            else:
                print("\nwaited too long - probably tx stuck by some reason")
                return False


def send_and_mine(tx_hex, rpc_connection):
    txid = rpc_connection.sendrawtransaction(tx_hex)
    assert txid, 'got txid'
    # we need the tx above to be confirmed in the next block
    assert mine_and_waitconfirms(txid, rpc_connection)
    return txid


def rpc_connect(rpc_user, rpc_password, ip, port):
    try:
        rpc_connection = Proxy("http://%s:%s@%s:%d" % (rpc_user, rpc_password, ip, port))
    except Exception:
        raise Exception("Connection error! Probably no daemon on selected port.")
    return rpc_connection


def wait_some_blocks(rpc_connection, blocks_to_wait):
    init_height = int(rpc_connection.getinfo()["blocks"])
    while True:
        current_height = int(rpc_connection.getinfo()["blocks"])
        height_difference = current_height - init_height
        if height_difference < blocks_to_wait:
            print("Waiting for more blocks")
            time.sleep(5)
        else:
            return True


def generate_random_string(length):
    random_string = ''.join(choice(ascii_uppercase) for i in range(length))
    return random_string


def komodo_teardown(*proxy_instances):
    for instance in proxy_instances:
        if type(instance) is list:
            for iteratable in instance:
                try:
                    isinstance(iteratable, Proxy)
                    iteratable.stop()
                except Exception as e:
                    raise TypeError("Not a Proxy object, error: " + str(e))
        else:
            try:
                isinstance(instance, Proxy)
                instance.stop()
            except Exception as e:
                raise TypeError("Not a Proxy object, error: " + str(e))

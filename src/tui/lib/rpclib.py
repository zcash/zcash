import http
from slickrpc import Proxy


# RPC connection
def rpc_connect(rpc_user, rpc_password, port):
    try:
        rpc_connection = Proxy("http://%s:%s@127.0.0.1:%d"%(rpc_user, rpc_password, port))
    except Exception:
        raise Exception("Connection error! Probably no daemon on selected port.")
    return rpc_connection


# Non CC calls
def getinfo(rpc_connection):
    try:
        getinfo = rpc_connection.getinfo()
    except Exception:
        raise Exception("Connection error!")
    return getinfo


def sendrawtransaction(rpc_connection, hex):
    tx_id = rpc_connection.sendrawtransaction(hex)
    return tx_id


def gettransaction(rpc_connection, tx_id):
    transaction_info = rpc_connection.gettransaction(tx_id)
    return transaction_info


def getrawtransaction(rpc_connection, tx_id):
    rawtransaction = rpc_connection.getrawtransaction(tx_id)
    return rawtransaction


def getbalance(rpc_connection):
    balance = rpc_connection.getbalance()
    return balance

# Token CC calls
def token_create(rpc_connection, name, supply, description):
    token_hex = rpc_connection.tokencreate(name, supply, description)
    return token_hex


def token_info(rpc_connection, token_id):
    token_info = rpc_connection.tokeninfo(token_id)
    return token_info


#TODO: have to add option with pubkey input
def token_balance(rpc_connection, token_id):
    token_balance = rpc_connection.tokenbalance(token_id)
    return token_balance

def token_list(rpc_connection):
    token_list = rpc_connection.tokenlist()
    return token_list


def token_convert(rpc_connection, evalcode, token_id, pubkey, supply):
    token_convert_hex = rpc_connection.tokenconvert(evalcode, token_id, pubkey, supply)
    return token_convert_hex

def get_rawmempool(rpc_connection):
    mempool = rpc_connection.getrawmempool()
    return mempool

# Oracle CC calls
def oracles_create(rpc_connection, name, description, data_type):
    oracles_hex = rpc_connection.oraclescreate(name, description, data_type)
    return oracles_hex


def oracles_register(rpc_connection, oracle_id, data_fee):
    oracles_register_hex = rpc_connection.oraclesregister(oracle_id, data_fee)
    return oracles_register_hex


def oracles_subscribe(rpc_connection, oracle_id, publisher_id, data_fee):
    oracles_subscribe_hex = rpc_connection.oraclessubscribe(oracle_id, publisher_id, data_fee)
    return oracles_subscribe_hex


def oracles_info(rpc_connection, oracle_id):
    oracles_info = rpc_connection.oraclesinfo(oracle_id)
    return oracles_info


def oracles_data(rpc_connection, oracle_id, hex_string):
    oracles_data = rpc_connection.oraclesdata(oracle_id, hex_string)
    return oracles_data


def oracles_list(rpc_connection):
    oracles_list = rpc_connection.oracleslist()
    return oracles_list


def oracles_samples(rpc_connection, oracletxid, batonutxo, num):
    oracles_sample = rpc_connection.oraclessamples(oracletxid, batonutxo, num)
    return oracles_sample


# Gateways CC calls
# Arguments changing dynamically depends of M N, so supposed to wrap it this way
# token_id, oracle_id, coin_name, token_supply, M, N + pubkeys for each N
def gateways_bind(rpc_connection, *args):
    gateways_bind_hex = rpc_connection.gatewaysbind(*args)
    return gateways_bind_hex


def gateways_deposit(rpc_connection, gateway_id, height, coin_name,\
                     coin_txid, claim_vout, deposit_hex, proof, dest_pub, amount):
    gateways_deposit_hex = rpc_connection.gatewaysdeposit(gateway_id, height, coin_name,\
                     coin_txid, claim_vout, deposit_hex, proof, dest_pub, amount)
    return gateways_deposit_hex


def gateways_claim(rpc_connection, gateway_id, coin_name, deposit_txid, dest_pub, amount):
    gateways_claim_hex = rpc_connection.gatewaysclaim(gateway_id, coin_name, deposit_txid, dest_pub, amount)
    return gateways_claim_hex


def gateways_withdraw(rpc_connection, gateway_id, coin_name, withdraw_pub, amount):
    gateways_withdraw_hex = rpc_connection.gatewayswithdraw(gateway_id, coin_name, withdraw_pub, amount)
    return gateways_withdraw_hex

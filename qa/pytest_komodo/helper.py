import os
import time
from decimal import *
from basic.pytest_util import validate_template
if os.name == 'posix':
    from slickrpc import Proxy
else:
    from bitcoinrpc.authproxy import AuthServiceProxy as Proxy


def main():
    node_params_dictionary1 = {
        "rpc_user": "test",
        "rpc_password": "test",
        "rpc_ip": "127.0.0.1",
        "rpc_port": 7000
    }
    rpc1 = Proxy("http://%s:%s@%s:%d" % (node_params_dictionary1.get('rpc_user'),
                                          node_params_dictionary1.get('rpc_password'),
                                          node_params_dictionary1.get('rpc_ip'),
                                          node_params_dictionary1.get('rpc_port')))

    node_params_dictionary2 = {
        "rpc_user": "test",
        "rpc_password": "test",
        "rpc_ip": "127.0.0.1",
        "rpc_port": 7001
    }
    rpc2 = Proxy("http://%s:%s@%s:%d" % (node_params_dictionary2.get('rpc_user'),
                                          node_params_dictionary2.get('rpc_password'),
                                          node_params_dictionary2.get('rpc_ip'),
                                          node_params_dictionary2.get('rpc_port')))

    #rpc1.setgenerate(True, 1)
    #rpc2.setgenerate(True, 1)
    #transparent1 = rpc1.getnewaddress()
    #print(transparent1)
    #shielded1 = rpc1.z_getnewaddress()
    #print(shielded1)
    #transparent2 = rpc2.getnewaddress()
    #print(transparent2)
    #shielded2 = rpc2.z_getnewaddress()
    #print(shielded2)
    #res = rpc1.z_gettotalbalance()
    #print(res)
    #res = rpc1.z_getbalance(shielded1)
    #print(res)
    #res = rpc1.getinfo().get("synced")
    #print(res)
    #res = rpc2.getinfo().get("synced")
    #print(res)
    #res = rpc1.listunspent()
    #print(res)
    #res = rpc2.z_listunspent()
    #print(res)
    #amount = rpc1.getbalance() / 1000
    #print(amount)
    #t_send = [{'address': 'zs10h8kdr3r98rkekfhn3za7mu4vjgpe4h96mwqk3ww2pt33n7jjzktrwhuksxqqd2ztg2awj9y8cp',
    #           'amount': '55,11'}]
    ##z_send = [{'address': shielded2, 'amount': amount}]
    #opid = rpc1.z_sendmany('RBvTN4GT9nVhH7BbJV2MYkfFLSwQvTA17H', t_send)
    ##print(opid)
    #res = rpc1.z_getoperationstatus([opid])
    #print(res)
    rpc1.stop()
    rpc2.stop()


if __name__ == '__main__':
    main()
